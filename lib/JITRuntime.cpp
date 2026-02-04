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
extern "C" uint64_t qore_rt_ref(uint64_t val) {
    QoreValue v = fromBits(val);
    if (v.hasNode()) {
        return toBits(v.refSelf());
    }
    return val;
}

// --- Arithmetic helpers ---

extern "C" uint64_t qore_rt_add_any(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalBinary(QoreIROpcode::AddAny, lv, rv, xsink);
    return toBits(result);
}

extern "C" uint64_t qore_rt_sub_any(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalBinary(QoreIROpcode::SubAny, lv, rv, xsink);
    return toBits(result);
}

extern "C" uint64_t qore_rt_mul_any(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalBinary(QoreIROpcode::MulAny, lv, rv, xsink);
    return toBits(result);
}

extern "C" uint64_t qore_rt_div_any(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalBinary(QoreIROpcode::DivAny, lv, rv, xsink);
    return toBits(result);
}

extern "C" uint64_t qore_rt_mod_any(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalBinary(QoreIROpcode::ModAny, lv, rv, xsink);
    return toBits(result);
}

// --- Integer/float division with zero check ---

extern "C" int64_t qore_rt_div_int(int64_t left, int64_t right, ExceptionSink* xsink) {
    if (!right) {
        if (xsink) {
            xsink->raiseException("DIVISION-BY-ZERO", "division by zero found in integer expression");
        }
        return 0;
    }
    return left / right;
}

extern "C" int64_t qore_rt_mod_int(int64_t left, int64_t right, ExceptionSink* xsink) {
    if (!right) {
        if (xsink) {
            xsink->raiseException("DIVISION-BY-ZERO", "modula operand cannot be zero");
        }
        return 0;
    }
    return left % right;
}

extern "C" double qore_rt_div_float(double left, double right, ExceptionSink* xsink) {
    if (right == 0.0) {
        if (xsink) {
            xsink->raiseException("DIVISION-BY-ZERO", "division by zero found in floating-point expression");
        }
        return 0.0;
    }
    return left / right;
}

// --- Conversion helpers ---

extern "C" int64_t qore_rt_to_int(uint64_t val) {
    QoreValue v = fromBits(val);
    return v.getAsBigInt();
}

extern "C" double qore_rt_to_float(uint64_t val) {
    QoreValue v = fromBits(val);
    return v.getAsFloat();
}

extern "C" int64_t qore_rt_to_bool(uint64_t val) {
    QoreValue v = fromBits(val);
    return v.getAsBool() ? 1 : 0;
}

// --- Refcount helpers ---

extern "C" void qore_rt_incref(uint64_t val) {
    QoreValue v = fromBits(val);
    if (v.hasNode()) {
        v.getInternalNode()->ref();
    }
}

extern "C" void qore_rt_decref(uint64_t val, ExceptionSink* xsink) {
    QoreValue v = fromBits(val);
    v.discard(xsink);
}

extern "C" void qore_rt_decref_nothrow(uint64_t val) {
    QoreValue v = fromBits(val);
    v.discard(nullptr);
}

// --- Exception helpers ---

extern "C" void qore_rt_throw(ExceptionSink* xsink, const char* err, const char* desc) {
    if (xsink) {
        xsink->raiseException(err, desc);
    }
}

extern "C" void qore_rt_throw_value(ExceptionSink* xsink, uint64_t val) {
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

extern "C" int64_t qore_rt_has_exception(ExceptionSink* xsink) {
    return (xsink && *xsink) ? 1 : 0;
}

// --- Invoke helpers ---

extern "C" uint64_t qore_rt_invoke_expr(uint64_t expr_bits, ExceptionSink* xsink) {
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

extern "C" uint64_t qore_rt_make_string(const char* str) {
    QoreStringNode* s = new QoreStringNode(str);
    QoreValue v(s);
    return toBits(v);
}

extern "C" uint64_t qore_rt_catch_exception(ExceptionSink* xsink) {
    if (!xsink || !*xsink) {
        return toBits(QoreValue());
    }
    QoreHashNode* info = xsink->getExceptionInfo();
    xsink->clear();
    if (info) {
        return toBits(QoreValue(info));
    }
    return toBits(QoreValue());
}

// --- Deopt helpers ---

extern "C" void qore_rt_deopt(void* deopt_counter_ptr) {
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

extern "C" int64_t qore_rt_guard_not_nothing(uint64_t val) {
    QoreValue v = fromBits(val);
    return v.isNothing() ? 0 : 1;
}

extern "C" int64_t qore_rt_guard_int(uint64_t val) {
    QoreValue v = fromBits(val);
    return v.isInt() ? 1 : 0;
}

extern "C" int64_t qore_rt_guard_float(uint64_t val) {
    QoreValue v = fromBits(val);
    return v.isFloat() ? 1 : 0;
}

// --- Local variable helpers ---

extern "C" void qore_rt_instantiate_local(LocalVar* var) {
    if (var) {
        var->instantiate(0);
    }
}

extern "C" void qore_rt_assign_local(LocalVar* var, uint64_t value, ExceptionSink* xsink) {
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

extern "C" uint64_t qore_rt_load_local(LocalVar* var, ExceptionSink* xsink) {
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

extern "C" void qore_rt_uninstantiate_local(LocalVar* var, ExceptionSink* xsink) {
    if (var) {
        var->uninstantiate(xsink);
    }
}

// --- Generic opcode dispatch helpers ---

extern "C" uint64_t qore_rt_binary_op(int opcode, uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalBinary(static_cast<QoreIROpcode>(opcode), lv, rv, xsink);
    return toBits(result);
}

extern "C" uint64_t qore_rt_unary_op(int opcode, uint64_t operand, ExceptionSink* xsink) {
    QoreValue val = fromBits(operand);
    QoreValue result = QoreIRInterpreter::evalUnary(static_cast<QoreIROpcode>(opcode), val, xsink);
    return toBits(result);
}

extern "C" uint64_t qore_rt_expr_op(int opcode, uint64_t expr_bits, ExceptionSink* xsink) {
    QoreValue expr = fromBits(expr_bits);
    QoreValue result = QoreIRInterpreter::evalExpr(static_cast<QoreIROpcode>(opcode), expr, xsink);
    return toBits(result);
}

extern "C" uint64_t qore_rt_comparison_op(int opcode, uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalComparison(static_cast<QoreIROpcode>(opcode), lv, rv, xsink);
    return toBits(result);
}

extern "C" uint64_t qore_rt_ternary_op(int opcode, uint64_t a, uint64_t b, uint64_t c, ExceptionSink* xsink) {
    QoreValue va = fromBits(a);
    QoreValue vb = fromBits(b);
    QoreValue vc = fromBits(c);
    QoreValue result = QoreIRInterpreter::evalTernary(static_cast<QoreIROpcode>(opcode), va, vb, vc, xsink);
    return toBits(result);
}

// --- Variable access helpers ---

extern "C" uint64_t qore_rt_load_global(Var* var, ExceptionSink* xsink) {
    if (!var) {
        return toBits(QoreValue());
    }
    // Var::eval() returns an already-referenced value
    QoreValue result = var->eval();
    return toBits(result);
}

extern "C" void qore_rt_store_global(Var* var, uint64_t value, ExceptionSink* xsink) {
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

extern "C" uint64_t qore_rt_load_closure(ClosureVarValue* var, ExceptionSink* xsink) {
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

extern "C" void qore_rt_store_closure(ClosureVarValue* var, uint64_t value, ExceptionSink* xsink) {
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

extern "C" uint64_t qore_rt_load_thread_local(Var* var, ExceptionSink* xsink) {
    // Thread-local variables use the same Var class; eval() resolves per-thread
    if (!var) {
        return toBits(QoreValue());
    }
    QoreValue result = var->eval();
    return toBits(result);
}

extern "C" void qore_rt_store_thread_local(Var* var, uint64_t value, ExceptionSink* xsink) {
    qore_rt_store_global(var, value, xsink);
}

// --- LValue operation helpers ---

extern "C" uint64_t qore_rt_lvalue_load(uint64_t lvalue_bits, ExceptionSink* xsink) {
    QoreValue lvalue = fromBits(lvalue_bits);
    QoreValue result = QoreIRInterpreter::evalLValueLoad(lvalue, xsink);
    return toBits(result);
}

extern "C" uint64_t qore_rt_lvalue_store(uint64_t lvalue_bits, uint64_t value_bits, ExceptionSink* xsink) {
    QoreValue lvalue = fromBits(lvalue_bits);
    QoreValue value = fromBits(value_bits);
    QoreValue result = QoreIRInterpreter::evalLValueStore(lvalue, value, xsink);
    return toBits(result);
}

extern "C" uint64_t qore_rt_lvalue_unary(int opcode, uint64_t lvalue_bits, ExceptionSink* xsink) {
    QoreValue lvalue = fromBits(lvalue_bits);
    QoreValue result = QoreIRInterpreter::evalLValueUnary(static_cast<QoreIROpcode>(opcode), lvalue, xsink);
    return toBits(result);
}

extern "C" uint64_t qore_rt_lvalue_binary(int opcode, uint64_t lvalue_bits, uint64_t value_bits,
        ExceptionSink* xsink) {
    QoreValue lvalue = fromBits(lvalue_bits);
    QoreValue value = fromBits(value_bits);
    QoreValue result = QoreIRInterpreter::evalLValueBinary(static_cast<QoreIROpcode>(opcode), lvalue, value, xsink);
    return toBits(result);
}

// --- Container construction helpers ---

extern "C" uint64_t qore_rt_make_list(uint64_t* vals, int count, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);
    for (int i = 0; i < count; i++) {
        QoreValue v = fromBits(vals[i]);
        if (v.hasNode()) {
            v.refSelf();
        }
        list->push(v, xsink);
    }
    return toBits(QoreValue(list.release()));
}

extern "C" uint64_t qore_rt_make_hash(uint64_t* kv_pairs, int count, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), xsink);
    // count is the number of key-value pairs; kv_pairs has 2*count elements
    for (int i = 0; i < count; i++) {
        QoreValue key = fromBits(kv_pairs[i * 2]);
        QoreValue val = fromBits(kv_pairs[i * 2 + 1]);
        QoreStringValueHelper key_str(key);
        if (val.hasNode()) {
            val.refSelf();
        }
        hash->setKeyValue(key_str->c_str(), val, xsink);
        if (*xsink) {
            return toBits(QoreValue());
        }
    }
    return toBits(QoreValue(hash.release()));
}

// --- Statement execution helpers ---

extern "C" uint64_t qore_rt_exec_statement(int opcode, const AbstractStatement* stmt, ExceptionSink* xsink) {
    if (!stmt) {
        return toBits(QoreValue());
    }
    QoreValue return_value;
    QoreIRInterpreter::execStatement(static_cast<QoreIROpcode>(opcode), stmt, return_value, xsink);
    return toBits(return_value);
}

extern "C" void qore_rt_thread_exit(ExceptionSink* xsink) {
    if (xsink) {
        xsink->raiseThreadExit();
    }
}

// --- Guard type helper ---

extern "C" int64_t qore_rt_guard_type(uint64_t val, const QoreTypeInfo* type_info) {
    QoreValue v = fromBits(val);
    return QoreTypeInfo::runtimeAcceptsValue(type_info, v) != QTI_NOT_EQUAL ? 1 : 0;
}

// --- Date construction helper ---

extern "C" uint64_t qore_rt_make_date(int64_t date_microseconds, int64_t is_relative) {
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

extern "C" uint64_t qore_rt_hash_key_access(uint64_t hash_val, const char* key, ExceptionSink* xsink) {
    QoreValue v = fromBits(hash_val);
    if (v.getType() == NT_HASH) {
        const QoreHashNode* h = v.get<const QoreHashNode>();
        QoreValue result = h->getKeyValue(key);
        return toBits(result.refSelf());
    }
    // Not a hash (or NOTHING/NULL): return NOTHING
    return toBits(QoreValue());
}

extern "C" uint64_t qore_rt_list_index_access(uint64_t list_val, int64_t index, ExceptionSink* xsink) {
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
extern "C" int64_t qore_rt_list_size(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() == NT_LIST) {
        return static_cast<int64_t>(v.get<const QoreListNode>()->size());
    }
    return 0;
}

extern "C" int64_t qore_rt_list_get_int(uint64_t list_val, int64_t index) {
    QoreValue v = fromBits(list_val);
    if (v.getType() == NT_LIST) {
        const QoreListNode* l = v.get<const QoreListNode>();
        if (index >= 0 && static_cast<size_t>(index) < l->size()) {
            return l->retrieveEntry(index).getAsBigInt();
        }
    }
    return 0;
}

extern "C" double qore_rt_list_get_float(uint64_t list_val, int64_t index) {
    QoreValue v = fromBits(list_val);
    if (v.getType() == NT_LIST) {
        const QoreListNode* l = v.get<const QoreListNode>();
        if (index >= 0 && static_cast<size_t>(index) < l->size()) {
            return l->retrieveEntry(index).getAsFloat();
        }
    }
    return 0.0;
}

// Optimized map operations - native loops that return lists
extern "C" uint64_t qore_rt_map_scale_int(uint64_t list_val, int64_t scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue(new QoreListNode(bigIntTypeInfo)));
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        result->push(l->retrieveEntry(i).getAsBigInt() * scale, nullptr);
    }
    return toBits(result.release());
}

extern "C" uint64_t qore_rt_map_scale_float(uint64_t list_val, double scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue(new QoreListNode(floatTypeInfo)));
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        result->push(l->retrieveEntry(i).getAsFloat() * scale, nullptr);
    }
    return toBits(result.release());
}

extern "C" uint64_t qore_rt_map_offset_int(uint64_t list_val, int64_t offset) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue(new QoreListNode(bigIntTypeInfo)));
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        result->push(l->retrieveEntry(i).getAsBigInt() + offset, nullptr);
    }
    return toBits(result.release());
}

extern "C" uint64_t qore_rt_map_offset_float(uint64_t list_val, double offset) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue(new QoreListNode(floatTypeInfo)));
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        result->push(l->retrieveEntry(i).getAsFloat() + offset, nullptr);
    }
    return toBits(result.release());
}

extern "C" uint64_t qore_rt_map_square_int(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue(new QoreListNode(bigIntTypeInfo)));
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

extern "C" uint64_t qore_rt_map_square_float(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue(new QoreListNode(floatTypeInfo)));
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

// Optimized select operations - native loops that filter lists
extern "C" uint64_t qore_rt_select_positive_int(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue(new QoreListNode(bigIntTypeInfo)));
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

extern "C" uint64_t qore_rt_select_positive_float(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue(new QoreListNode(floatTypeInfo)));
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

extern "C" uint64_t qore_rt_select_nonzero_int(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue(new QoreListNode(bigIntTypeInfo)));
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

extern "C" uint64_t qore_rt_select_nonzero_float(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue(new QoreListNode(floatTypeInfo)));
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
extern "C" uint64_t qore_rt_fused_map_select_scale_positive_int(uint64_t list_val, int64_t scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue(new QoreListNode(bigIntTypeInfo)));
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

extern "C" uint64_t qore_rt_fused_map_select_scale_positive_float(uint64_t list_val, double scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue(new QoreListNode(floatTypeInfo)));
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

extern "C" uint64_t qore_rt_fused_map_select_offset_positive_int(uint64_t list_val, int64_t offset) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue(new QoreListNode(bigIntTypeInfo)));
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

extern "C" uint64_t qore_rt_fused_map_select_offset_positive_float(uint64_t list_val, double offset) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue(new QoreListNode(floatTypeInfo)));
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

extern "C" uint64_t qore_rt_fused_map_select_square_positive_int(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue(new QoreListNode(bigIntTypeInfo)));
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

extern "C" uint64_t qore_rt_fused_map_select_square_positive_float(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue(new QoreListNode(floatTypeInfo)));
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

extern "C" uint64_t qore_rt_string_concat(uint64_t left, uint64_t right, ExceptionSink* xsink) {
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

// --- DotEval with pre-evaluated base helper ---

#include "qore/intern/QoreDotEvalOperatorNode.h"

extern "C" uint64_t qore_rt_dot_eval_with_base(uint64_t expr_bits, uint64_t base_bits, ExceptionSink* xsink) {
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

extern "C" uint64_t qore_rt_call_with_args(uint64_t expr_bits, uint64_t* args, int nargs, ExceptionSink* xsink) {
    QoreValue expr = fromBits(expr_bits);
    if (!expr.hasNode()) {
        return toBits(QoreValue());
    }

    // Build QoreListNode from the NaN-boxed args array
    ReferenceHolder<QoreListNode> arg_list(new QoreListNode(autoTypeInfo), xsink);
    for (int i = 0; i < nargs; ++i) {
        QoreValue val = fromBits(args[i]);
        if (val.hasNode()) {
            val.refSelf();
        }
        arg_list->push(val, xsink);
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

// --- On-block-exit handler support for JIT ---

struct JITOnBlockExitHandler {
    obe_type_e type;
    StatementBlock* code;
};

static thread_local std::vector<JITOnBlockExitHandler> jit_obe_handlers;

extern "C" void qore_rt_push_on_block_exit(int type, StatementBlock* code) {
    jit_obe_handlers.push_back({static_cast<obe_type_e>(type), code});
}

extern "C" int64_t qore_rt_get_on_block_exit_count() {
    return static_cast<int64_t>(jit_obe_handlers.size());
}

extern "C" void qore_rt_exec_on_block_exit(int64_t saved_count, ExceptionSink* xsink) {
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
            if (jit_obe_handlers[i].code) {
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
                QoreValue rv;
                jit_obe_handlers[i].code->exec(rv, &obe_xsink);
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
                rv.discard(nullptr);
            }
        }
    }

    // Remove handlers for this function scope
    jit_obe_handlers.resize(start);
}

// --- AOT context-based helpers (Phase 7b) ---

#include "qore/intern/QoreAOT.h"

extern "C" void qore_rt_push_on_block_exit_aot(QoreAOTContext* ctx, int32_t idx, int type) {
    assert(ctx && idx >= 0 && idx < ctx->num_stmts);
    qore_rt_push_on_block_exit(type, ctx->stmts[idx]);
}

extern "C" uint64_t qore_rt_load_local_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    return qore_rt_load_local(ctx->locals[idx], xsink);
}

extern "C" void qore_rt_assign_local_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    qore_rt_assign_local(ctx->locals[idx], val, xsink);
}

extern "C" void qore_rt_instantiate_local_aot(QoreAOTContext* ctx, int32_t idx) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    qore_rt_instantiate_local(ctx->locals[idx]);
}

extern "C" void qore_rt_uninstantiate_local_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    qore_rt_uninstantiate_local(ctx->locals[idx], xsink);
}

extern "C" uint64_t qore_rt_load_global_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_globals);
    return qore_rt_load_global(ctx->globals[idx], xsink);
}

extern "C" void qore_rt_store_global_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_globals);
    qore_rt_store_global(ctx->globals[idx], val, xsink);
}

extern "C" uint64_t qore_rt_load_thread_local_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_globals);
    return qore_rt_load_thread_local(ctx->globals[idx], xsink);
}

extern "C" void qore_rt_store_thread_local_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_globals);
    qore_rt_store_thread_local(ctx->globals[idx], val, xsink);
}

extern "C" uint64_t qore_rt_load_closure_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    return qore_rt_load_local(ctx->locals[idx], xsink);
}

extern "C" void qore_rt_store_closure_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    qore_rt_assign_local(ctx->locals[idx], val, xsink);
}

extern "C" uint64_t qore_rt_invoke_expr_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    return qore_rt_invoke_expr(ctx->exprs[idx], xsink);
}

extern "C" uint64_t qore_rt_lvalue_load_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    return qore_rt_lvalue_load(ctx->exprs[idx], xsink);
}

extern "C" uint64_t qore_rt_lvalue_store_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    return qore_rt_lvalue_store(ctx->exprs[idx], val, xsink);
}

extern "C" uint64_t qore_rt_lvalue_unary_aot(int op, QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    return qore_rt_lvalue_unary(op, ctx->exprs[idx], xsink);
}

extern "C" uint64_t qore_rt_lvalue_binary_aot(int op, QoreAOTContext* ctx, int32_t idx, uint64_t val,
        ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    return qore_rt_lvalue_binary(op, ctx->exprs[idx], val, xsink);
}

extern "C" uint64_t qore_rt_call_with_args_aot(QoreAOTContext* ctx, int32_t slot, uint64_t* args, int nargs,
        ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
    return qore_rt_call_with_args(ctx->exprs[slot], args, nargs, xsink);
}

extern "C" uint64_t qore_rt_dot_eval_with_base_aot(QoreAOTContext* ctx, int32_t slot, uint64_t base_bits,
        ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
    return qore_rt_dot_eval_with_base(ctx->exprs[slot], base_bits, xsink);
}

// --- Regex op with pre-evaluated operand helper ---

#include "qore/intern/QoreRegexMatchOperatorNode.h"
#include "qore/intern/QoreRegexExtractOperatorNode.h"
#include "qore/intern/QoreRegexNMatchOperatorNode.h"
#include "qore/intern/QoreRegex.h"

extern "C" uint64_t qore_rt_regex_op_with_operand(int32_t opcode, uint64_t expr_bits, uint64_t operand_bits,
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

extern "C" uint64_t qore_rt_regex_op_with_operand_aot(QoreAOTContext* ctx, int32_t opcode, int32_t slot,
        uint64_t operand_bits, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
    return qore_rt_regex_op_with_operand(opcode, ctx->exprs[slot], operand_bits, xsink);
}

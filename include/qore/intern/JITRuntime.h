/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    JITRuntime.h

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

#ifndef _QORE_INTERN_JITRUNTIME_H
#define _QORE_INTERN_JITRUNTIME_H

#include <cstdint>

class ExceptionSink;

// C ABI helpers called by JIT-generated code.
// All functions use extern "C" linkage so LLVM can resolve them by name.
//
// QoreValue is NaN-boxed in a uint64_t for the JIT ABI:
//   - int64_t values are encoded via QoreValue(int64) constructor
//   - double values are encoded via QoreValue(double) constructor
//   - bool values are encoded via QoreValue(bool) constructor
//   - NOTHING is encoded as 0
//
// The JIT represents QoreValue as i64 in LLVM IR.  These helpers accept
// and return raw uint64_t bit patterns that correspond to NaN-boxed QoreValues.

extern "C" {

// --- Arithmetic helpers (for .any ops that need dynamic dispatch) ---

//! Add two QoreValues with dynamic type dispatch
uint64_t qore_rt_add_any(uint64_t left, uint64_t right, ExceptionSink* xsink);

//! Subtract two QoreValues with dynamic type dispatch
uint64_t qore_rt_sub_any(uint64_t left, uint64_t right, ExceptionSink* xsink);

//! Multiply two QoreValues with dynamic type dispatch
uint64_t qore_rt_mul_any(uint64_t left, uint64_t right, ExceptionSink* xsink);

//! Divide two QoreValues with dynamic type dispatch
uint64_t qore_rt_div_any(uint64_t left, uint64_t right, ExceptionSink* xsink);

//! Modulo two QoreValues with dynamic type dispatch
uint64_t qore_rt_mod_any(uint64_t left, uint64_t right, ExceptionSink* xsink);

// --- Integer division with zero check ---

//! Divide two int64_t values, raising DIVISION-BY-ZERO if divisor is 0
int64_t qore_rt_div_int(int64_t left, int64_t right, ExceptionSink* xsink);

//! Modulo two int64_t values, raising DIVISION-BY-ZERO if divisor is 0
int64_t qore_rt_mod_int(int64_t left, int64_t right, ExceptionSink* xsink);

//! Divide two doubles, raising DIVISION-BY-ZERO if divisor is 0
double qore_rt_div_float(double left, double right, ExceptionSink* xsink);

// --- Conversion helpers ---

//! Convert a QoreValue to int64_t
int64_t qore_rt_to_int(uint64_t val);

//! Convert a QoreValue to double
double qore_rt_to_float(uint64_t val);

//! Convert a QoreValue to bool
int64_t qore_rt_to_bool(uint64_t val);

// --- Refcount helpers ---

//! Increment reference count if value is a pointer type
void qore_rt_incref(uint64_t val);

//! Decrement reference count; may throw via xsink
void qore_rt_decref(uint64_t val, ExceptionSink* xsink);

//! Decrement reference count; never throws (for landing pad cleanup)
void qore_rt_decref_nothrow(uint64_t val);

// --- Exception helpers ---

//! Raise a Qore exception; err and desc are C strings
void qore_rt_throw(ExceptionSink* xsink, const char* err, const char* desc);

//! Raise a Qore exception from a NaN-boxed QoreValue (list of err, desc[, arg])
void qore_rt_throw_value(ExceptionSink* xsink, uint64_t val);

//! Check if an exception has been raised
int64_t qore_rt_has_exception(ExceptionSink* xsink);

// --- Invoke helpers ---

//! Invoke a QoreValue expression node (AST evaluation).
//! expr_ptr is the raw pointer to the QoreValue::bits field holding the expression.
//! Returns the NaN-boxed result; sets xsink on exception.
uint64_t qore_rt_invoke_expr(uint64_t expr_bits, ExceptionSink* xsink);

//! Create a QoreStringNode from a C string and return as NaN-boxed QoreValue.
uint64_t qore_rt_make_string(const char* str);

//! Get exception info hash from ExceptionSink; returns NaN-boxed QoreValue (hash or NOTHING).
//! Also clears the exception from the sink.
uint64_t qore_rt_catch_exception(ExceptionSink* xsink);

// --- Deopt helpers ---

//! Deopt entrypoint: transfer back to IR interpreter.
//! deopt_id identifies the deopt point metadata.
//! Returns the result of IR interpreter execution as a NaN-boxed QoreValue.
uint64_t qore_rt_deopt(uint64_t deopt_id, ExceptionSink* xsink);

// --- Guard helpers ---

//! Check if a NaN-boxed QoreValue is not NOTHING; returns 1 if not NOTHING, 0 otherwise
int64_t qore_rt_guard_not_nothing(uint64_t val);

//! Check if a NaN-boxed QoreValue is an int; returns 1 if int, 0 otherwise
int64_t qore_rt_guard_int(uint64_t val);

//! Check if a NaN-boxed QoreValue is a float; returns 1 if float, 0 otherwise
int64_t qore_rt_guard_float(uint64_t val);

// --- Local variable helpers ---
// These keep the Qore thread-local variable stack in sync so that AST
// callbacks (via qore_rt_invoke_expr) can resolve local variables.

class LocalVar;
class Var;
class ClosureVarValue;
class AbstractStatement;
class QoreTypeInfo;

//! Instantiate a local variable on the Qore thread-local variable stack.
//! Must be called once per local at JIT function entry before any loads/stores.
void qore_rt_instantiate_local(LocalVar* var);

//! Assign a NaN-boxed QoreValue to a local variable on the Qore thread-local
//! variable stack.  The JIT should call this on every StoreLocal so the Qore
//! runtime stays in sync.
void qore_rt_assign_local(LocalVar* var, uint64_t value, ExceptionSink* xsink);

//! Load a NaN-boxed QoreValue from a local variable on the Qore thread-local
//! variable stack.
uint64_t qore_rt_load_local(LocalVar* var, ExceptionSink* xsink);

//! Uninstantiate a local variable from the Qore thread-local variable stack.
//! Must be called once per local at JIT function exit.
//! Accepts the LocalVar* to dispatch correctly between lvstack and cvstack
//! based on the variable's closure_use flag.
void qore_rt_uninstantiate_local(LocalVar* var, ExceptionSink* xsink);

// --- Generic opcode dispatch helpers ---
// These delegate to QoreIRInterpreter eval methods for opcodes that don't
// have dedicated native LLVM implementations.

//! Generic binary op: delegates to QoreIRInterpreter::evalBinary()
uint64_t qore_rt_binary_op(int opcode, uint64_t left, uint64_t right, ExceptionSink* xsink);

//! Generic unary op: delegates to QoreIRInterpreter::evalUnary()
uint64_t qore_rt_unary_op(int opcode, uint64_t operand, ExceptionSink* xsink);

//! Generic expression op: delegates to QoreIRInterpreter::evalExpr()
uint64_t qore_rt_expr_op(int opcode, uint64_t expr_bits, ExceptionSink* xsink);

//! Generic comparison op: delegates to QoreIRInterpreter::evalComparison()
uint64_t qore_rt_comparison_op(int opcode, uint64_t left, uint64_t right, ExceptionSink* xsink);

//! Generic ternary op: delegates to QoreIRInterpreter::evalTernary()
uint64_t qore_rt_ternary_op(int opcode, uint64_t a, uint64_t b, uint64_t c, ExceptionSink* xsink);

// --- Variable access helpers ---

//! Load from a global variable; returns NaN-boxed QoreValue
uint64_t qore_rt_load_global(Var* var, ExceptionSink* xsink);

//! Store a NaN-boxed QoreValue to a global variable
void qore_rt_store_global(Var* var, uint64_t value, ExceptionSink* xsink);

//! Load from a closure variable; returns NaN-boxed QoreValue
uint64_t qore_rt_load_closure(ClosureVarValue* var, ExceptionSink* xsink);

//! Store a NaN-boxed QoreValue to a closure variable
void qore_rt_store_closure(ClosureVarValue* var, uint64_t value, ExceptionSink* xsink);

//! Load from a thread-local variable; returns NaN-boxed QoreValue
uint64_t qore_rt_load_thread_local(Var* var, ExceptionSink* xsink);

//! Store a NaN-boxed QoreValue to a thread-local variable
void qore_rt_store_thread_local(Var* var, uint64_t value, ExceptionSink* xsink);

// --- LValue operation helpers ---

//! LValue load: evaluates lvalue expression, returns NaN-boxed value
uint64_t qore_rt_lvalue_load(uint64_t lvalue_bits, ExceptionSink* xsink);

//! LValue store: assigns value to lvalue, returns NaN-boxed result
uint64_t qore_rt_lvalue_store(uint64_t lvalue_bits, uint64_t value_bits, ExceptionSink* xsink);

//! LValue unary op (++, --): returns NaN-boxed result
uint64_t qore_rt_lvalue_unary(int opcode, uint64_t lvalue_bits, ExceptionSink* xsink);

//! LValue binary op (+=, -=, etc.): returns NaN-boxed result
uint64_t qore_rt_lvalue_binary(int opcode, uint64_t lvalue_bits, uint64_t value_bits, ExceptionSink* xsink);

// --- Container construction helpers ---

//! MakeList: takes array of NaN-boxed values, returns NaN-boxed list
uint64_t qore_rt_make_list(uint64_t* values, int count, ExceptionSink* xsink);

//! MakeHash: takes array of key-value pairs (alternating), returns NaN-boxed hash
uint64_t qore_rt_make_hash(uint64_t* kv_pairs, int count, ExceptionSink* xsink);

// --- Statement execution helpers ---

//! Execute a statement (foreach, on_block_exit, context, etc.)
//! Returns NaN-boxed result (NOTHING for void statements)
uint64_t qore_rt_exec_statement(int opcode, const AbstractStatement* stmt, ExceptionSink* xsink);

//! Signal thread exit via ExceptionSink
void qore_rt_thread_exit(ExceptionSink* xsink);

// --- On-block-exit handler helpers ---
// These manage deferred on_error/on_exit/on_success handlers for JIT-compiled
// functions.  Each JIT function saves the handler count at entry and executes
// (in LIFO order) any handlers registered during its execution at return time.

class StatementBlock;

//! Register an on_block_exit handler for deferred execution.
//! type: OBE_Unconditional=0, OBE_Success=1, OBE_Error=2
void qore_rt_push_on_block_exit(int type, StatementBlock* code);

//! Get current handler count (called at function entry to save base).
int64_t qore_rt_get_on_block_exit_count();

//! Execute on_block_exit handlers registered since saved_count, in LIFO order.
//! Removes executed handlers from the stack.
void qore_rt_exec_on_block_exit(int64_t saved_count, ExceptionSink* xsink);

// --- Guard type helper ---

//! Check if value matches the given type; returns 1 if match, 0 otherwise
int64_t qore_rt_guard_type(uint64_t val, const QoreTypeInfo* type_info);

// --- Date construction helper ---

//! Create a DateTimeNode from epoch microseconds and relative flag; returns NaN-boxed
uint64_t qore_rt_make_date(int64_t date_microseconds, int64_t is_relative);

// --- Specialized access helpers (Phase 5b optimizations) ---

//! Look up a key in a hash value; returns NaN-boxed result (with ref).
//! Falls back to NOTHING if value is not a hash or key doesn't exist.
uint64_t qore_rt_hash_key_access(uint64_t hash_val, const char* key, ExceptionSink* xsink);

//! Index into a list value; returns NaN-boxed result (with ref).
//! Returns NOTHING if value is not a list or index is out of bounds.
uint64_t qore_rt_list_index_access(uint64_t list_val, int64_t index, ExceptionSink* xsink);

//! Concatenate two string values; returns NaN-boxed new string (with ref).
//! Falls back to qore_rt_add_any if either operand is not a string.
uint64_t qore_rt_string_concat(uint64_t left, uint64_t right, ExceptionSink* xsink);

// --- AOT context-based helpers (Phase 7b) ---
// These variants take QoreAOTContext* and a slot index instead of raw pointers.
// At runtime, they resolve ctx->array[idx] and delegate to the existing helpers.

struct QoreAOTContext;

//! Load from a local variable via AOT context slot
uint64_t qore_rt_load_local_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);

//! Assign to a local variable via AOT context slot
void qore_rt_assign_local_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink);

//! Instantiate a local variable via AOT context slot
void qore_rt_instantiate_local_aot(QoreAOTContext* ctx, int32_t idx);

//! Uninstantiate a local variable via AOT context slot
void qore_rt_uninstantiate_local_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);

//! Load from a global variable via AOT context slot
uint64_t qore_rt_load_global_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);

//! Store to a global variable via AOT context slot
void qore_rt_store_global_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink);

//! Load from a thread-local variable via AOT context slot
uint64_t qore_rt_load_thread_local_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);

//! Store to a thread-local variable via AOT context slot
void qore_rt_store_thread_local_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink);

//! Load from a closure variable via AOT context slot (uses locals array)
uint64_t qore_rt_load_closure_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);

//! Store to a closure variable via AOT context slot (uses locals array)
void qore_rt_store_closure_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink);

//! Invoke an expression via AOT context slot
uint64_t qore_rt_invoke_expr_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);

//! LValue load via AOT context slot
uint64_t qore_rt_lvalue_load_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);

//! LValue store via AOT context slot
uint64_t qore_rt_lvalue_store_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink);

//! LValue unary op via AOT context slot
uint64_t qore_rt_lvalue_unary_aot(int op, QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);

//! LValue binary op via AOT context slot
uint64_t qore_rt_lvalue_binary_aot(int op, QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink);

//! Invoke a DotEval expression with a pre-evaluated base value.
//! expr_bits is the NaN-boxed expression node (QoreDotEvalOperatorNode).
//! base_bits is the NaN-boxed pre-evaluated base QoreValue.
//! Returns the NaN-boxed result; sets xsink on exception.
uint64_t qore_rt_dot_eval_with_base(uint64_t expr_bits, uint64_t base_bits, ExceptionSink* xsink);

//! AOT variant: resolve expression from context slot, then delegate to qore_rt_dot_eval_with_base
uint64_t qore_rt_dot_eval_with_base_aot(QoreAOTContext* ctx, int32_t slot, uint64_t base_bits,
    ExceptionSink* xsink);

//! Invoke a call-type expression with pre-evaluated arguments.
//! expr_bits is the NaN-boxed expression node (FunctionCallNode, SelfFunctionCallNode,
//! StaticMethodCallNode, or CallReferenceCallNode).
//! args is an array of nargs NaN-boxed QoreValues.
//! Returns the NaN-boxed result; sets xsink on exception.
uint64_t qore_rt_call_with_args(uint64_t expr_bits, uint64_t* args, int nargs, ExceptionSink* xsink);

//! AOT variant: resolve expression from context slot, then delegate to qore_rt_call_with_args
uint64_t qore_rt_call_with_args_aot(QoreAOTContext* ctx, int32_t slot, uint64_t* args, int nargs,
    ExceptionSink* xsink);

//! Regex op with pre-evaluated operand: evaluates regex match/extract using the given operand
//! instead of re-evaluating the AST subject expression.
//! opcode identifies the regex operation (RegexMatchAny, RegexMatchBool, RegexNMatchBool,
//! RegexExtractAny, RegexExtractList).
//! expr_bits is the NaN-boxed regex operator expression node.
//! operand_bits is the NaN-boxed pre-evaluated subject value.
//! Returns the NaN-boxed result; sets xsink on exception.
uint64_t qore_rt_regex_op_with_operand(int32_t opcode, uint64_t expr_bits, uint64_t operand_bits,
    ExceptionSink* xsink);

//! AOT variant: resolve expression from context slot, then delegate to qore_rt_regex_op_with_operand
uint64_t qore_rt_regex_op_with_operand_aot(QoreAOTContext* ctx, int32_t opcode, int32_t slot,
    uint64_t operand_bits, ExceptionSink* xsink);

} // extern "C"

#endif

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

#include <cstddef>
#include <cstdint>
#include <string>

class ExceptionSink;
class QoreIRFunction;
class QoreIRLValuePathInstruction;
class QoreValue;
class QoreVarInfo;
class UserVariantBase;

struct QoreJITRuntimeSymbolInfo {
    const char* name;
    void* address;
};

//! Returns the central JIT runtime helper symbol registry.
const QoreJITRuntimeSymbolInfo* qore_jit_get_runtime_symbols(size_t& count);

//! Validates the central JIT runtime helper symbol registry.
bool qore_jit_validate_runtime_symbols(std::string& error);

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

//! Check the current native stack guard for generated code entry points
int qore_rt_check_stack(ExceptionSink* xsink);

// --- Arithmetic helpers (for .any ops that need dynamic dispatch) ---

//! Add two QoreValues with dynamic type dispatch
uint64_t qore_rt_add_any(uint64_t left, uint64_t right, ExceptionSink* xsink);

//! Subtract two QoreValues with dynamic type dispatch
uint64_t qore_rt_sub_any(uint64_t left, uint64_t right, ExceptionSink* xsink);

//! Multiply two QoreValues with dynamic type dispatch
uint64_t qore_rt_mul_any(uint64_t left, uint64_t right, ExceptionSink* xsink);

//! Add two QoreValues with compound-assignment dynamic dispatch
uint64_t qore_rt_add_assign_any(uint64_t left, uint64_t right, ExceptionSink* xsink);

//! Subtract two QoreValues with compound-assignment dynamic dispatch
uint64_t qore_rt_sub_assign_any(uint64_t left, uint64_t right, ExceptionSink* xsink);

//! Multiply two QoreValues with compound-assignment dynamic dispatch
uint64_t qore_rt_mul_assign_any(uint64_t left, uint64_t right, ExceptionSink* xsink);

uint64_t qore_rt_add_assign_any_throwing(uint64_t left, uint64_t right, ExceptionSink* xsink);
uint64_t qore_rt_sub_assign_any_throwing(uint64_t left, uint64_t right, ExceptionSink* xsink);
uint64_t qore_rt_mul_assign_any_throwing(uint64_t left, uint64_t right, ExceptionSink* xsink);

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

//! Return true if a NaN-boxed QoreValue is NULL or NOTHING
int64_t qore_rt_is_null_or_nothing(uint64_t val);

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

//! Fail-fast guard for any remaining generic expression invocation path.
//! Native IR/JIT/AOT lowering must not execute arbitrary AST expressions.
uint64_t qore_rt_invoke_expr(uint64_t expr_bits, ExceptionSink* xsink);

//! Create a QoreStringNode from a C string and return as NaN-boxed QoreValue.
uint64_t qore_rt_make_string(const char* str);

//! Create a QoreStringNode from string bytes and return as NaN-boxed QoreValue.
uint64_t qore_rt_make_string_len(const char* str, uint64_t len);

//! Execute a backquote command and return stdout as a NaN-boxed string.
uint64_t qore_rt_backquote(const char* cmd, ExceptionSink* xsink);

//! Throwing wrapper for invoke/EH paths.
uint64_t qore_rt_backquote_throwing(const char* cmd, ExceptionSink* xsink);

//! Execute a find expression from serialized sub-expression values.
uint64_t qore_rt_find(uint64_t exp_bits, uint64_t find_exp_bits, uint64_t where_bits,
    ExceptionSink* xsink);

//! Execute a find expression from serialized sub-expression values with an explicit mode.
uint64_t qore_rt_find_mode(uint64_t exp_bits, uint64_t find_exp_bits, uint64_t where_bits,
    int32_t mode, ExceptionSink* xsink);

//! Throwing wrapper for invoke/EH paths.
uint64_t qore_rt_find_throwing(uint64_t exp_bits, uint64_t find_exp_bits, uint64_t where_bits,
    ExceptionSink* xsink);

//! Throwing wrapper for mode-aware find invoke/EH paths.
uint64_t qore_rt_find_mode_throwing(uint64_t exp_bits, uint64_t find_exp_bits, uint64_t where_bits,
    int32_t mode, ExceptionSink* xsink);

//! Native AOT background obj.method(args) call with pre-evaluated receiver and args.
uint64_t qore_rt_background_dot_eval_name_call_aot(const char* method_name, uint64_t recv_bits,
    uint64_t* args, int nargs, ExceptionSink* xsink);

//! Throwing wrapper for EH paths.
uint64_t qore_rt_background_dot_eval_name_call_aot_throwing(const char* method_name,
    uint64_t recv_bits, uint64_t* args, int nargs, ExceptionSink* xsink);

//! Native AOT background Class::method(args) call with pre-evaluated args.
uint64_t qore_rt_background_static_method_name_call_aot(const char* qualified_name,
    uint64_t* args, int nargs, ExceptionSink* xsink);

//! Throwing wrapper for EH paths.
uint64_t qore_rt_background_static_method_name_call_aot_throwing(const char* qualified_name,
    uint64_t* args, int nargs, ExceptionSink* xsink);

//! Native AOT background call-reference/closure invocation with pre-evaluated args.
uint64_t qore_rt_background_call_ref_value_aot(uint64_t callee_bits, uint64_t* args,
    int nargs, ExceptionSink* xsink);

//! Throwing wrapper for EH paths.
uint64_t qore_rt_background_call_ref_value_aot_throwing(uint64_t callee_bits,
    uint64_t* args, int nargs, ExceptionSink* xsink);

//! Get exception info hash from ExceptionSink; returns NaN-boxed QoreValue (hash or NOTHING).
//! Also clears the exception from the sink and sets td->catchException for rethrow support.
uint64_t qore_rt_catch_exception(ExceptionSink* xsink);

//! Clean up catch scope: restore previous td->catchException and delete the caught exception.
//! Called at the end of each catch block (try.merge) and before function return from within catch.
void qore_rt_catch_end(ExceptionSink* xsink);

//! Rethrow the current catch exception: copies exception into xsink and cleans up catch scope.
void qore_rt_rethrow(ExceptionSink* xsink);

// --- Deopt helpers ---

//! Atomically increment the deopt counter for a variant.
//! Called from JIT-compiled profiled guard failure paths.
//! The evalTiered path checks this counter to trigger JIT recompilation.
void qore_rt_deopt(void* deopt_counter_ptr);

// --- Guard helpers ---

//! Check if a NaN-boxed QoreValue is not NOTHING; returns 1 if not NOTHING, 0 otherwise
int64_t qore_rt_guard_not_nothing(uint64_t val);

//! Raise the runtime type error required when a non-NOTHING return yields NOTHING.
void qore_rt_raise_return_nothing(ExceptionSink* xsink);

//! Check if a NaN-boxed QoreValue is an int; returns 1 if int, 0 otherwise
int64_t qore_rt_guard_int(uint64_t val);

//! Check if a NaN-boxed QoreValue is a float; returns 1 if float, 0 otherwise
int64_t qore_rt_guard_float(uint64_t val);

// --- Boxing helpers ---

//! Box a native int64_t into a NaN-boxed QoreValue.
//! Handles values outside the 48-bit inline range by allocating a QoreBigIntNode.
uint64_t qore_rt_box_big_int(int64_t val);

// --- Local variable helpers ---
// These keep the Qore thread-local variable stack in sync for runtime helpers,
// closures, reference arguments, and dynamic calls that resolve locals via TLS.

class LocalVar;
class Var;
class ClosureVarValue;
class AbstractStatement;
class QoreTypeInfo;
class TypedHashDecl;

//! Instantiate a local variable on the Qore thread-local variable stack.
//! Must be called once per local at JIT function entry before any loads/stores.
void qore_rt_instantiate_local(LocalVar* var);

//! Assign a NaN-boxed QoreValue to a local variable on the Qore thread-local
//! variable stack.  The JIT should call this on every StoreLocal so the Qore
//! runtime stays in sync.
void qore_rt_assign_local(LocalVar* var, uint64_t value, ExceptionSink* xsink);
void qore_rt_assign_local_throwing(LocalVar* var, uint64_t value, ExceptionSink* xsink);

//! Assign a NaN-boxed QoreValue to a local variable without type coercion.
//! Used when coercion has already been applied (e.g., via qore_rt_coerce_value).
//! This avoids double-coercion for complex-typed locals.
void qore_rt_assign_local_no_coerce(LocalVar* var, uint64_t value, ExceptionSink* xsink);
void qore_rt_assign_local_no_coerce_throwing(LocalVar* var, uint64_t value, ExceptionSink* xsink);

//! Publish a compiled local alloca value to the runtime local stack before
//! executing deferred handlers.  This intentionally ignores any pre-existing
//! exception in the caller's ExceptionSink because on_error handlers run while
//! that exception is active.
void qore_rt_sync_local(LocalVar* var, uint64_t value);

//! Wrap an object/hash/list value in a weak reference for weak assignment.
//! Returns an owned value: a new weak-reference node for supported weak types,
//! or an extra reference to unsupported node values.
uint64_t qore_rt_make_weak_value(uint64_t value, ExceptionSink* xsink);

//! Load a NaN-boxed QoreValue from a local variable on the Qore thread-local
//! variable stack.
uint64_t qore_rt_load_local(LocalVar* var, ExceptionSink* xsink);

//! Run all cleanup actions from an array of pointers to NaN-boxed cleanup slots.
void qore_rt_cleanup_run_allocas(uint64_t** alloca_ptrs, int32_t count, ExceptionSink* xsink);

//! Clear caller-owned argument cleanup slots after callee ownership has been established.
void qore_rt_clear_arg_cleanups(uint64_t** arg_cleanups, int32_t count, ExceptionSink* xsink);

//! Refresh a compiled local cache from the Qore runtime stack if its valid
//! epoch is stale.  Used by LLVM lowering to keep cache invalidation compact.
void qore_rt_reload_local_if_stale(LocalVar* var, uint64_t* cache, uint64_t* tracker,
        uint64_t* deferred, uint64_t* valid_epoch, uint64_t epoch, ExceptionSink* xsink);

//! Clear a local variable's value on the Qore thread-local variable stack
//! without popping the stack entry.  This triggers destructors (via decref)
//! at block scope exit for pre-instantiated locals whose stack entry must
//! remain for the caller to clean up.
//! Uses raw stack lookup + del(xsink) — bypasses LValueHelper::assign()
//! which asserts !*xsink (unsafe when a prior destructor already set xsink).
void qore_rt_clear_local(LocalVar* var, ExceptionSink* xsink);

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

//! Generic square-bracket access with caller-selected string index semantics.
uint64_t qore_rt_list_index_dynamic(uint64_t left, uint64_t right, int32_t string_index_char, ExceptionSink* xsink);

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

//! Add a native integer delta to a typed local through its runtime lvalue
int64_t qore_rt_add_assign_local_int(LocalVar* var, int64_t delta, ExceptionSink* xsink);

//! Increment a typed integer closure variable through its runtime LocalVar binding
int64_t qore_rt_increment_closure_int(LocalVar* var, int64_t delta, ExceptionSink* xsink);

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

//! LValue ternary op (splice): returns NaN-boxed result
uint64_t qore_rt_lvalue_ternary(int opcode, uint64_t lvalue_bits, uint64_t first_bits, uint64_t second_bits,
    uint64_t third_bits, ExceptionSink* xsink);

// --- Container construction helpers ---

//! MakeList: takes array of NaN-boxed values with optional parse-time type info, returns NaN-boxed list
uint64_t qore_rt_make_list(uint64_t* values, int count, const QoreTypeInfo* typeInfo, ExceptionSink* xsink);
uint64_t qore_rt_make_list_by_type_path(uint64_t* values, int count, const char* type_path, ExceptionSink* xsink);
uint64_t qore_rt_make_list_by_type_path_throwing(uint64_t* values, int count, const char* type_path,
        ExceptionSink* xsink);

//! MakeHash: takes array of key-value pairs (alternating) with optional parse-time type info, returns NaN-boxed hash
uint64_t qore_rt_make_hash(uint64_t* kv_pairs, int count, const QoreTypeInfo* typeInfo, ExceptionSink* xsink);
uint64_t qore_rt_make_hash_by_type_path(uint64_t* kv_pairs, int count, const char* type_path, ExceptionSink* xsink);
uint64_t qore_rt_make_hash_by_type_path_throwing(uint64_t* kv_pairs, int count, const char* type_path,
        ExceptionSink* xsink);

//! MakeHashConstKeys: takes array of const keys and NaN-boxed values with optional parse-time type info, returns NaN-boxed hash
uint64_t qore_rt_make_hash_const_keys(const char** keys, uint64_t* vals, int count, const QoreTypeInfo* typeInfo, ExceptionSink* xsink);
uint64_t qore_rt_make_hash_const_keys_by_type_path(const char** keys, uint64_t* vals, int count,
        const char* type_path, ExceptionSink* xsink);
uint64_t qore_rt_make_hash_const_keys_by_type_path_throwing(const char** keys, uint64_t* vals, int count,
        const char* type_path, ExceptionSink* xsink);

//! Convert an assigned native integer to a referenced string without boxing it first.
uint64_t qore_rt_int_to_string(int64_t value);

//! Return the byte and character length of an integer's decimal string form.
int64_t qore_rt_int_to_string_measure(int64_t value);

//! Measure a concatenation without materializing the result when all parts have
//! the same encoding. @p characters selects character length instead of bytes.
int64_t qore_rt_string_concat_multi_measure(uint64_t* args, int nargs,
        int32_t characters, ExceptionSink* xsink);

//! Execute a proven two-key hash-to-hash remap without a Qore call frame. Non-hash
//! inputs use the original AOT call slot so parameter binding remains authoritative.
uint64_t qore_rt_fixed_hash_remap2_aot(QoreAOTContext* ctx, int32_t slot, uint64_t value,
        const char* input_key1, uint64_t input_hash1_64, uint32_t input_hash1_32,
        const char* output_key1, const char* input_key2, uint64_t input_hash2_64,
        uint32_t input_hash2_32, const char* output_key2, const char* type_path,
        ExceptionSink* xsink);
uint64_t qore_rt_fixed_hash_remap2_aot_throwing(QoreAOTContext* ctx, int32_t slot,
        uint64_t value, const char* input_key1, uint64_t input_hash1_64,
        uint32_t input_hash1_32, const char* output_key1, const char* input_key2,
        uint64_t input_hash2_64, uint32_t input_hash2_32, const char* output_key2,
        const char* type_path, ExceptionSink* xsink);

//! Create an empty list with an optional element type; nullptr means auto.
uint64_t qore_rt_create_empty_list_typed(const QoreTypeInfo* element_type, ExceptionSink* xsink);

//! AOT-safe variant of qore_rt_create_empty_list_typed() using a serialized element type path.
uint64_t qore_rt_create_empty_list_by_type_path(const char* element_type_path, ExceptionSink* xsink);

//! Push to a list with an optional element type used when auto-vivifying from NOTHING.
uint64_t qore_rt_list_push_typed(uint64_t list_bits, uint64_t val_bits, const QoreTypeInfo* element_type,
        ExceptionSink* xsink);
uint64_t qore_rt_list_push_typed_throwing(uint64_t list_bits, uint64_t val_bits, const QoreTypeInfo* element_type,
        ExceptionSink* xsink);

//! AOT-safe list push variant using a serialized element type path.
uint64_t qore_rt_list_push_by_type_path(uint64_t list_bits, uint64_t val_bits, const char* element_type_path,
        ExceptionSink* xsink);
uint64_t qore_rt_list_push_by_type_path_throwing(uint64_t list_bits, uint64_t val_bits,
        const char* element_type_path, ExceptionSink* xsink);

//! Push to a proven assigned list and return the same borrowed list value.
uint64_t qore_rt_list_push_in_place(uint64_t list_bits, uint64_t val_bits, ExceptionSink* xsink);
uint64_t qore_rt_list_push_in_place_throwing(uint64_t list_bits, uint64_t val_bits, ExceptionSink* xsink);

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

//! Register an on_block_exit handler with compiled IR for IR interpreter execution.
void qore_rt_push_on_block_exit_ir(int type, StatementBlock* code, const QoreIRFunction* handler_ir);

//! Register an on_block_exit handler with a natively compiled LLVM function.
//! compiled_fn is the native function pointer; handler_func provides all_body_locals
//! for pre-instantiation/uninstantiation of handler locals.
void qore_rt_push_compiled_handler(int type, StatementBlock* code,
        uint64_t (*compiled_fn)(ExceptionSink*), const QoreIRFunction* handler_func);

//! Get current handler count (called at function entry to save base).
int64_t qore_rt_get_on_block_exit_count();

//! Execute on_block_exit handlers registered since saved_count, in LIFO order.
//! Removes executed handlers from the stack.
void qore_rt_exec_on_block_exit(int64_t saved_count, ExceptionSink* xsink);

//! Discard (without firing) on_block_exit handlers registered since saved_count,
//! truncating the thread-local handler stack back to that mark.  Used by the JIT
//! deopt path before re-executing via the AST interpreter.
void qore_rt_discard_on_block_exit(int64_t saved_count);

struct QoreAOTContext;

//! Register an on_block_exit handler via AOT context slot; raises if handler IR is missing.
void qore_rt_push_on_block_exit_aot(QoreAOTContext* ctx, int32_t idx, int type, ExceptionSink* xsink);

//! AOT slot-indexed variant of qore_rt_sync_local().
void qore_rt_sync_local_aot(QoreAOTContext* ctx, int32_t idx, uint64_t value);

//! Begin an exact parent slot cache for deferred handler IR executed by a
//! native/JIT/AOT parent.  Returns an opaque guard consumed by
//! qore_rt_end_native_ir_slot_cache().
void* qore_rt_begin_native_ir_slot_cache(int32_t count);

//! Populate one native/JIT parent slot cache entry from a raw LocalVar*.
void qore_rt_set_native_ir_slot_cache_value(void* guard, int32_t ir_slot, LocalVar* var, uint64_t value);

//! Populate one native/AOT parent slot cache entry from an AOT local slot.
void qore_rt_set_native_ir_slot_cache_value_aot(QoreAOTContext* ctx, void* guard,
        int32_t ir_slot, int32_t local_slot, uint64_t value);

//! End a native parent slot cache and publish any handler write-backs to TLS.
void qore_rt_end_native_ir_slot_cache(void* guard, ExceptionSink* xsink);

// --- Guard type helper ---

//! Check if value matches the given type; returns 1 if match, 0 otherwise
int64_t qore_rt_guard_type(uint64_t val, const QoreTypeInfo* type_info);

// --- Date construction helper ---

//! Create a DateTimeNode from epoch microseconds and relative flag; returns NaN-boxed
uint64_t qore_rt_make_date(int64_t date_microseconds, int64_t is_relative);

//! Create a DateTimeNode from complete date constant metadata; returns NaN-boxed
uint64_t qore_rt_make_date_ex(int64_t date_microseconds, int64_t is_relative, const char* zone_name,
        int64_t rel_years, int64_t rel_months, int64_t rel_days, int64_t rel_hours,
        int64_t rel_minutes, int64_t rel_seconds, int64_t rel_us);

// --- Enum construction helper ---

//! Create a TAG_ENUM QoreValue from a QoreEnumMember pointer; returns NaN-boxed
uint64_t qore_rt_make_enum(int64_t member_ptr);

// --- Constant loading helper ---

class RuntimeConstantRefNode;

//! Load a runtime constant value; returns NaN-boxed QoreValue
uint64_t qore_rt_load_constant(const RuntimeConstantRefNode* node, ExceptionSink* xsink);

//! Load a runtime constant or direct constant value via AOT context slot
uint64_t qore_rt_load_constant_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);

//! Load a member from the current runtime object; returns NaN-boxed QoreValue.
uint64_t qore_rt_load_self_member(const char* member_name, ExceptionSink* xsink);

//! Load a member for use as a method-call base; preserves raw weak-reference results.
uint64_t qore_rt_load_self_member_for_call(const char* member_name, ExceptionSink* xsink);

//! Load a static class variable by class path and variable name; returns NaN-boxed QoreValue
uint64_t qore_rt_load_static_var(QoreVarInfo* vi, const char* var_name, ExceptionSink* xsink);
uint64_t qore_rt_load_static_var_for_call(QoreVarInfo* vi, const char* var_name, ExceptionSink* xsink);
uint64_t qore_rt_load_static_var_throwing(QoreVarInfo* vi, const char* var_name, ExceptionSink* xsink);
uint64_t qore_rt_load_static_var_for_call_throwing(QoreVarInfo* vi, const char* var_name,
        ExceptionSink* xsink);
uint64_t qore_rt_load_static_var_by_path(const char* class_path, const char* var_name, ExceptionSink* xsink);
uint64_t qore_rt_load_static_var_by_path_for_call(const char* class_path, const char* var_name,
        ExceptionSink* xsink);
uint64_t qore_rt_load_static_var_by_path_throwing(const char* class_path, const char* var_name,
        ExceptionSink* xsink);
uint64_t qore_rt_load_static_var_by_path_for_call_throwing(const char* class_path, const char* var_name,
        ExceptionSink* xsink);
uint64_t qore_rt_load_static_var_by_path_aot(QoreAOTContext* ctx, const char* class_path,
        const char* var_name, ExceptionSink* xsink);
uint64_t qore_rt_load_static_var_by_path_for_call_aot(QoreAOTContext* ctx, const char* class_path,
        const char* var_name, ExceptionSink* xsink);
uint64_t qore_rt_load_static_var_by_path_aot_throwing(QoreAOTContext* ctx, const char* class_path,
        const char* var_name, ExceptionSink* xsink);
uint64_t qore_rt_load_static_var_by_path_for_call_aot_throwing(QoreAOTContext* ctx, const char* class_path,
        const char* var_name, ExceptionSink* xsink);

// --- Closure creation helper ---

class QoreClosureParseNode;

//! Create a closure/lambda; returns NaN-boxed QoreValue
uint64_t qore_rt_create_closure(const QoreClosureParseNode* cn, ExceptionSink* xsink);

//! Create a closure/lambda via AOT context slot
uint64_t qore_rt_create_closure_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);

//! Call a single-use noncapturing closure without materializing a runtime closure node.
uint64_t qore_rt_call_immediate_closure(const QoreClosureParseNode* cn,
        uint64_t* args, int nargs, ExceptionSink* xsink);
uint64_t qore_rt_call_immediate_closure_consume_args(const QoreClosureParseNode* cn,
        uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink);
uint64_t qore_rt_call_immediate_closure_aot(QoreAOTContext* ctx, int32_t idx,
        uint64_t* args, int nargs, ExceptionSink* xsink);
uint64_t qore_rt_call_immediate_closure_aot_consume_args(QoreAOTContext* ctx,
        int32_t idx, uint64_t* args, uint64_t** arg_cleanups, int nargs,
        ExceptionSink* xsink);

// --- Reference creation helpers ---

class ParseReferenceNode;

//! Create a call reference (function/static method); returns NaN-boxed QoreValue
uint64_t qore_rt_create_call_ref(uint64_t expr_bits, ExceptionSink* xsink);

//! Create a class-qualified method call reference from serialized AOT metadata.
uint64_t qore_rt_create_static_method_call_ref_aot(const char* class_path, const char* method_name,
    ExceptionSink* xsink);
uint64_t qore_rt_create_static_method_call_ref_aot_throwing(const char* class_path, const char* method_name,
    ExceptionSink* xsink);

//! Create a resolved local method call reference from serialized AOT metadata.
uint64_t qore_rt_create_local_method_call_ref_aot(const char* class_path, const char* method_name,
    ExceptionSink* xsink);
uint64_t qore_rt_create_local_method_call_ref_aot_throwing(const char* class_path, const char* method_name,
    ExceptionSink* xsink);

//! Create a function call reference from serialized AOT metadata.
uint64_t qore_rt_create_function_call_ref_aot(const char* function_name, ExceptionSink* xsink);
uint64_t qore_rt_create_function_call_ref_aot_throwing(const char* function_name, ExceptionSink* xsink);

//! Create a method reference; returns NaN-boxed QoreValue
uint64_t qore_rt_create_method_ref(uint64_t expr_bits, ExceptionSink* xsink);

//! Create a self method reference by method name; returns NaN-boxed QoreValue
uint64_t qore_rt_create_self_method_ref_aot(const char* method_name, ExceptionSink* xsink);
uint64_t qore_rt_create_self_method_ref_aot_throwing(const char* method_name, ExceptionSink* xsink);

//! Create an object method reference from an evaluated object expression; returns NaN-boxed QoreValue
uint64_t qore_rt_create_object_method_ref_aot(uint64_t object_bits, const char* method_name,
    ExceptionSink* xsink);
uint64_t qore_rt_create_object_method_ref_aot_throwing(uint64_t object_bits, const char* method_name,
    ExceptionSink* xsink);

//! Create a parse reference (\var); returns NaN-boxed QoreValue
uint64_t qore_rt_create_parse_ref(const ParseReferenceNode* node, ExceptionSink* xsink);
uint64_t qore_rt_create_parse_ref_resolved_hash_key(const ParseReferenceNode* node, uint64_t key_bits,
    ExceptionSink* xsink);

//! Create a parse reference via AOT context slot
uint64_t qore_rt_create_parse_ref_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);
uint64_t qore_rt_create_parse_ref_aot_resolved_hash_key(QoreAOTContext* ctx, int32_t idx, uint64_t key_bits,
    ExceptionSink* xsink);

//! Create a local-variable parse reference via an AOT local slot.
uint64_t qore_rt_create_local_ref_aot(QoreAOTContext* ctx, int32_t local_slot, ExceptionSink* xsink);

//! Create a self-member hash parse reference via AOT metadata.
uint64_t qore_rt_create_member_hash_ref_aot(const char* member_name, uint64_t key_bits,
    const char* ref_type_path, ExceptionSink* xsink);

// --- Typed container construction helpers ---

class NewHashDeclNode;
class NewComplexHashNode;
class NewComplexListNode;
class NewComplexBufferNode;

//! Create a new hashdecl instance; returns NaN-boxed QoreValue
uint64_t qore_rt_new_hash_decl(const NewHashDeclNode* node, ExceptionSink* xsink);

//! Create a new hashdecl instance via AOT context slot
uint64_t qore_rt_new_hash_decl_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);

//! Create a new typed hash; returns NaN-boxed QoreValue
uint64_t qore_rt_new_complex_hash(const NewComplexHashNode* node, ExceptionSink* xsink);

//! Create a new typed hash via AOT context slot
uint64_t qore_rt_new_complex_hash_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);

//! Create a new typed list; returns NaN-boxed QoreValue
uint64_t qore_rt_new_complex_list(const NewComplexListNode* node, ExceptionSink* xsink);

//! Create a new typed list via AOT context slot
uint64_t qore_rt_new_complex_list_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);

//! Create a new typed buffer; returns NaN-boxed QoreValue
uint64_t qore_rt_new_complex_buffer(const NewComplexBufferNode* node, ExceptionSink* xsink);

//! Create a new typed buffer via AOT context slot
uint64_t qore_rt_new_complex_buffer_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);

//! Create a new typed hash from an already-evaluated hash initializer
uint64_t qore_rt_new_hash_decl_from_hash(const TypedHashDecl* hd, uint64_t hash_bits, int32_t runtime_check,
    ExceptionSink* xsink);
uint64_t qore_rt_new_hash_decl_from_hash_by_path_cached(QoreAOTContext* ctx, const char* hd_path,
    uint64_t hash_bits, int32_t runtime_check, ExceptionSink* xsink);
uint64_t qore_rt_new_complex_hash_from_hash(const QoreTypeInfo* typeInfo, uint64_t hash_bits, ExceptionSink* xsink);
uint64_t qore_rt_new_complex_hash_from_hash_by_type_path(const char* type_path, uint64_t hash_bits,
    ExceptionSink* xsink);

//! Create a new typed list from an already-evaluated constructor value
uint64_t qore_rt_new_complex_list_from_value(const QoreTypeInfo* typeInfo, uint64_t value_bits, ExceptionSink* xsink);
uint64_t qore_rt_new_complex_list_from_value_by_type_path(const char* type_path, uint64_t value_bits,
    ExceptionSink* xsink);

//! Create a new typed buffer from an already-evaluated constructor value
uint64_t qore_rt_new_complex_buffer_from_value(const QoreTypeInfo* typeInfo, uint64_t value_bits,
    ExceptionSink* xsink);
uint64_t qore_rt_new_complex_buffer_from_value_by_type_path(const char* type_path, uint64_t value_bits,
    ExceptionSink* xsink);
uint64_t qore_rt_new_complex_buffer_from_value_kind(const QoreTypeInfo* typeInfo, uint64_t value_bits,
    int32_t init_kind, ExceptionSink* xsink);
uint64_t qore_rt_new_complex_buffer_from_value_kind_by_type_path(const char* type_path, uint64_t value_bits,
    int32_t init_kind, ExceptionSink* xsink);

class VarRefNewObjectNode;

//! Construct value for VarRefNewObjectNode (non-object types: hashdecl/complex hash/list)
uint64_t qore_rt_vrn_construct(const VarRefNewObjectNode* vrn, ExceptionSink* xsink);

// --- Hash building helper ---

//! Set a key-value pair in a hash (for hash map loops)
void qore_rt_hash_set_key_value(uint64_t hash_bits, uint64_t key_bits, uint64_t value_bits,
    ExceptionSink* xsink);
//! Reserves storage in a newly constructed hash for a nonnegative capacity hint.
void qore_rt_hash_reserve(uint64_t hash_bits, int64_t capacity);

// --- Reverse iterator creation helper ---

//! Create a reverse iterator from a list/iterable (for foldr); returns opaque iterator pointer
void* qore_rt_iterator_create_reverse(uint64_t iterable_bits, ExceptionSink* xsink);

//! Create an iterator with `iterate` source semantics; returns opaque iterator pointer
void* qore_rt_iterator_create_iterate(uint64_t iterable_bits, ExceptionSink* xsink);
void* qore_rt_iterator_create_iterate_throwing(uint64_t iterable_bits, ExceptionSink* xsink);

//! Evaluate `iterate <source>` as an AbstractIterator object.
uint64_t qore_rt_iterate_value(uint64_t source_bits, ExceptionSink* xsink);
uint64_t qore_rt_iterate_value_throwing(uint64_t source_bits, ExceptionSink* xsink);

// --- Iterator cleanup helper ---

//! Clean up an active iterator on non-normal function exit (return/throw inside foreach body)
void qore_rt_iterator_cleanup(void* iter_ptr);

// --- Reference foreach helpers ---

//! Initialize reference foreach state from a ParseReferenceNode expression.
//! Returns an opaque state pointer (as uint64_t), or 0 on error.
uint64_t qore_rt_ref_foreach_init(uint64_t parse_ref_bits, ExceptionSink* xsink);

//! Get the iteration count for a reference foreach state.
int64_t qore_rt_ref_foreach_size(uint64_t state_ptr);

//! Get the element at the given index from the reference foreach state.
uint64_t qore_rt_ref_foreach_get_entry(uint64_t state_ptr, int64_t index, ExceptionSink* xsink);

//! Record the modified loop variable value after body execution.
void qore_rt_ref_foreach_record(uint64_t state_ptr, uint64_t value_bits, ExceptionSink* xsink);

//! Finalize: optionally fill remaining elements (on break), write back to reference, and clean up.
void qore_rt_ref_foreach_finalize(uint64_t state_ptr, int64_t fill_remaining, ExceptionSink* xsink);

//! Clean up reference foreach state without write-back (exception/early-exit paths).
void qore_rt_ref_foreach_cleanup(uint64_t state_ptr, ExceptionSink* xsink);

// --- Native `context` statement helpers ---

//! Create a Context frame from the data expression + optional where/sort filters.
//! Pushes onto the thread-local context stack.  Returns Context* as uint64_t,
//! or 0 on failure (xsink is set; no destroy needed).
uint64_t qore_rt_context_init(const char* name, uint64_t exp_bits, uint64_t where_bits,
    uint64_t sort_bits, int sort_type, ExceptionSink* xsink);

//! Throwing wrapper (invoke-based EH path).  Calls qore_rt_context_init and
//! throws QoreJITException on xsink-set failure so the LLVM invoke landingpad
//! fires.
uint64_t qore_rt_context_init_throwing(const char* name, uint64_t exp_bits, uint64_t where_bits,
    uint64_t sort_bits, int sort_type, ExceptionSink* xsink);

//! Evaluate `%field` / `context:field` against the thread-local context stack.
uint64_t qore_rt_context_ref_at(const char* key, int32_t stack_offset, ExceptionSink* xsink);

//! Throwing wrapper for context references.
uint64_t qore_rt_context_ref_at_throwing(const char* key, int32_t stack_offset,
    ExceptionSink* xsink);

//! Evaluate `%%` and return a new hash for the current context row.
uint64_t qore_rt_context_row(ExceptionSink* xsink);

//! Throwing wrapper for context row references.
uint64_t qore_rt_context_row_throwing(ExceptionSink* xsink);

//! Get the iteration count (max_pos) from a Context state handle.  Nothrow.
int64_t qore_rt_context_max_pos(uint64_t state_ptr);

//! Set the current row position on a Context state handle.  Nothrow.
void qore_rt_context_set_pos(uint64_t state_ptr, int64_t index);

//! Pop + free a Context frame.  Safe on null / already-destroyed.  Nothrow
//! (pending xsink on entry is preserved; the hash deref may enqueue a further
//! exception but won't overwrite a pre-existing one).
void qore_rt_context_destroy(uint64_t state_ptr, ExceptionSink* xsink);

// --- Specialized access helpers (Phase 5b optimizations) ---

//! Look up a key in a hash value; returns NaN-boxed result (with ref).
//! Falls back to NOTHING if value is not a hash or key doesn't exist.
uint64_t qore_rt_hash_key_access(uint64_t hash_val, const char* key, ExceptionSink* xsink);

//! Look up a key for use as a method-call base; preserves raw weak-reference results.
uint64_t qore_rt_hash_key_access_for_call(uint64_t hash_val, const char* key, ExceptionSink* xsink);

//! Constant-key hash lookup using hashes calculated by IR/AOT lowering.
uint64_t qore_rt_hash_key_access_prehashed(uint64_t hash_val, const char* key,
        uint64_t hash64, uint32_t hash32, ExceptionSink* xsink);
uint64_t qore_rt_hash_key_access_for_call_prehashed(uint64_t hash_val, const char* key,
        uint64_t hash64, uint32_t hash32, ExceptionSink* xsink);

//! Constant-key access when the operand is proven to be an assigned hash.
uint64_t qore_rt_hash_key_access_hash(uint64_t hash_val, const char* key, ExceptionSink* xsink);
uint64_t qore_rt_hash_key_access_hash_prehashed(uint64_t hash_val, const char* key,
        uint64_t hash64, uint32_t hash32, ExceptionSink* xsink);

//! Constant-key access on an explicit hash type that may still be NOTHING.
uint64_t qore_rt_hash_key_access_hash_guarded(uint64_t hash_val, const char* key, ExceptionSink* xsink);
uint64_t qore_rt_hash_key_access_hash_guarded_prehashed(uint64_t hash_val, const char* key,
        uint64_t hash64, uint32_t hash32, ExceptionSink* xsink);

//! Test the truth value of a constant hash key without materializing an owned
//! lookup result. Guarded variants accept NOTHING and other non-hash values.
int64_t qore_rt_hash_key_truthy(uint64_t hash_val, const char* key, ExceptionSink* xsink);
int64_t qore_rt_hash_key_truthy_prehashed(uint64_t hash_val, const char* key,
        uint64_t hash64, uint32_t hash32, ExceptionSink* xsink);
int64_t qore_rt_hash_key_truthy_guarded(uint64_t hash_val, const char* key,
        ExceptionSink* xsink);
int64_t qore_rt_hash_key_truthy_guarded_prehashed(uint64_t hash_val, const char* key,
        uint64_t hash64, uint32_t hash32, ExceptionSink* xsink);

//! Look up a key known to have an int value; returns NOTHING if unavailable.
uint64_t qore_rt_hash_key_access_int(uint64_t hash_val, const char* key);
uint64_t qore_rt_hash_key_access_int_prehashed(uint64_t hash_val, const char* key,
        uint64_t hash64, uint32_t hash32);

//! Select list hash elements whose constant-key value converts to an integer > 0.
uint64_t qore_rt_select_hash_key_positive_int(uint64_t list_val, const char* key,
        ExceptionSink* xsink);
uint64_t qore_rt_select_hash_key_positive_int_prehashed(uint64_t list_val,
        const char* key, uint64_t hash64, uint32_t hash32, ExceptionSink* xsink);

//! Map `hash-key + integer` over a typed hash list while preserving dynamic addition semantics.
uint64_t qore_rt_map_hash_key_offset_any(uint64_t list_val, const char* key,
        int64_t offset, ExceptionSink* xsink);
uint64_t qore_rt_map_hash_key_offset_any_prehashed(uint64_t list_val, const char* key,
        uint64_t hash64, uint32_t hash32, int64_t offset, ExceptionSink* xsink);

//! Index into a list value; returns NaN-boxed result (with ref).
//! Returns NOTHING if value is not a list or index is out of bounds.
uint64_t qore_rt_list_index_access(uint64_t list_val, int64_t index, ExceptionSink* xsink);
uint64_t qore_rt_list_index_access_compat(uint64_t list_val, int64_t index, int32_t string_index_char,
        ExceptionSink* xsink);

//! Extract the value assigned to one LHS entry in a list assignment.
uint64_t qore_rt_list_assignment_value(uint64_t value, int64_t index, ExceptionSink* xsink);
uint64_t qore_rt_list_assignment_value_throwing(uint64_t value, int64_t index, ExceptionSink* xsink);

//! Concatenate two string values; returns NaN-boxed new string (with ref).
//! Falls back to qore_rt_add_any if either operand is not a string.
uint64_t qore_rt_string_concat(uint64_t left, uint64_t right, ExceptionSink* xsink);

//! Fold an exact list<string> with a constant separator; returns a referenced string or NOTHING.
uint64_t qore_rt_foldl_string_join_checked(uint64_t list_val, uint64_t separator_val,
        ExceptionSink* xsink);

//! Start and extend an accumulator owned exclusively by fused string-join IR.
uint64_t qore_rt_string_join_start(uint64_t first, uint64_t separator, uint64_t value,
        ExceptionSink* xsink);
uint64_t qore_rt_string_join_append(uint64_t accumulator, uint64_t separator, uint64_t value,
        ExceptionSink* xsink);

//! Format one boxed integer according to compiler-validated fixed %d metadata.
uint64_t qore_rt_sprintf_int_fixed(uint64_t literal, uint64_t value, int64_t metadata);

//! Append a string value in place when the left string has one owner, or copy it otherwise.
uint64_t qore_rt_string_append_cow(uint64_t left, uint64_t right, ExceptionSink* xsink);

// --- Optimized list iteration helpers (higher-order optimization) ---

//! Get list size; returns 0 if value is not a list.
int64_t qore_rt_list_size(uint64_t list_val);

//! Get int element at index; returns 0 if not a list or index out of bounds.
int64_t qore_rt_list_get_int(uint64_t list_val, int64_t index);

//! Get float element at index; returns 0.0 if not a list or index out of bounds.
double qore_rt_list_get_float(uint64_t list_val, int64_t index);

//! Get an int element when the compiler has proven list type and index bounds.
int64_t qore_rt_list_get_int_unchecked(uint64_t list_val, int64_t index);

//! Get a float element when the compiler has proven list type and index bounds.
double qore_rt_list_get_float_unchecked(uint64_t list_val, int64_t index);

//! Get the immutable entry array when the compiler has proven list type and loop stability.
const uint64_t* qore_rt_list_get_data_unchecked(uint64_t list_val);

//! Raise the typed foreach assignment error for an unassigned typed list slot.
void qore_rt_raise_typed_foreach_nothing(int32_t value_kind, ExceptionSink* xsink);

//! Get the mutable entry array for a fresh fixed-size list owned by generated code.
uint64_t* qore_rt_list_get_mutable_data_unchecked(uint64_t list_val);

//! Finalize the logical length of a fresh list after direct scalar stores.
void qore_rt_list_set_length_unchecked(uint64_t list_val, int64_t length);

//! Get any element at index; returns NaN-boxed QoreValue (with +1 ref).
//! Returns NOTHING if not a list or index out of bounds.
uint64_t qore_rt_list_get_value(uint64_t list_val, int64_t index, ExceptionSink* xsink);

//! Return a list element borrowed from the source list without incrementing its reference count.
uint64_t qore_rt_list_get_value_noref(uint64_t list_val, int64_t index, ExceptionSink* xsink);

//! Create a list with pre-allocated capacity; returns NaN-boxed QoreListNode*.
uint64_t qore_rt_create_sized_list(int64_t capacity, ExceptionSink* xsink);
uint64_t qore_rt_create_sized_list_typed(int64_t capacity, const QoreTypeInfo* element_type, ExceptionSink* xsink);
uint64_t qore_rt_create_sized_list_by_type_path(int64_t capacity, const char* element_type_path,
        ExceptionSink* xsink);
//! Create an exact typed list at its final size for direct scalar map output stores.
uint64_t qore_rt_create_fixed_list_typed(int64_t size, const QoreTypeInfo* element_type,
        ExceptionSink* xsink);
uint64_t qore_rt_create_fixed_list_by_type_path(int64_t size, const char* element_type_path,
        ExceptionSink* xsink);

//! Set int element in list at index (for pre-sized typed map output). No bounds check.
void qore_rt_list_set_int(uint64_t list_bits, int64_t index, int64_t value);

//! Set float element in list at index (for pre-sized typed map output). No bounds check.
void qore_rt_list_set_float(uint64_t list_bits, int64_t index, double value);

//! Set any element in list at index (for pre-sized typed map output). No bounds check.
//! Takes ownership of the reference (no additional ref needed).
void qore_rt_list_set_value(uint64_t list_bits, int64_t index, uint64_t value_bits);

//! Checked variant preserving typed list assignment and exception semantics.
//! Takes ownership of the value reference.
void qore_rt_list_set_value_checked(uint64_t list_bits, int64_t index, uint64_t value_bits,
        ExceptionSink* xsink);
void qore_rt_list_set_value_checked_throwing(uint64_t list_bits, int64_t index,
        uint64_t value_bits, ExceptionSink* xsink);

//! Increment reference count for heap-allocated values; no-op for inline values.
uint64_t qore_rt_refself(uint64_t bits);

//! Get runtime class pointer from an object value; returns 0 if not an object.
uint64_t qore_rt_get_object_class(uint64_t obj_bits);

//! Fast closure/callref call with 0 arguments — bypasses QoreListNode + dynamic_cast
uint64_t qore_rt_call_closure_0(uint64_t ref_bits, ExceptionSink* xsink);

//! Fast closure/callref call with 1 argument — bypasses QoreListNode + dynamic_cast
uint64_t qore_rt_call_closure_1(uint64_t ref_bits, uint64_t arg0_bits, ExceptionSink* xsink);

//! Fast closure/callref call with N arguments — bypasses QoreListNode + dynamic_cast
uint64_t qore_rt_call_closure_fast(uint64_t ref_bits, uint64_t* args, int nargs, ExceptionSink* xsink);

//! Fast closure/callref call with N arguments, consuming caller-owned argument cleanup refs.
uint64_t qore_rt_call_closure_fast_consume_args(uint64_t ref_bits, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink);

// --- AOT context-based helpers (Phase 7b) ---
// These variants take QoreAOTContext* and a slot index instead of raw pointers.
// At runtime, they resolve ctx->array[idx] and delegate to the existing helpers.

struct QoreAOTContext;

//! Load from a local variable via AOT context slot
uint64_t qore_rt_load_local_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);

//! AOT variant of qore_rt_reload_local_if_stale().
void qore_rt_reload_local_if_stale_aot(QoreAOTContext* ctx, int32_t idx, uint64_t* cache,
        uint64_t* tracker, uint64_t* deferred, uint64_t* valid_epoch, uint64_t epoch,
        ExceptionSink* xsink);

//! Assign to a local variable via AOT context slot
void qore_rt_assign_local_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink);
void qore_rt_assign_local_aot_throwing(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink);

//! Assign to a local variable via AOT context slot without type coercion.
//! Used when coercion has already been applied.
void qore_rt_assign_local_no_coerce_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink);
void qore_rt_assign_local_no_coerce_aot_throwing(QoreAOTContext* ctx, int32_t idx, uint64_t val,
        ExceptionSink* xsink);

//! Instantiate a local variable via AOT context slot
void qore_rt_instantiate_local_aot(QoreAOTContext* ctx, int32_t idx);

//! Clear a local variable's value via AOT context slot (block scope exit)
void qore_rt_clear_local_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);

//! Uninstantiate a local variable via AOT context slot (clear only, no pop)
void qore_rt_uninstantiate_local_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);

//! Pop a closure-use variable from the cvstack via AOT context slot
//! Unlike qore_rt_uninstantiate_local_aot (which only clears), this properly pops
//! the ClosureVarValue from the thread-local closure variable stack.
//! Used for closure-use vars that are NOT pre-instantiated by evalTiered.
void qore_rt_pop_closure_var_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);

//! Load from a global variable via AOT context slot
uint64_t qore_rt_load_global_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);

//! Load an integer global without boxing; assigned receives 1, 0 for NOTHING, or -1 for fallback
int64_t qore_rt_load_global_int_aot(QoreAOTContext* ctx, int32_t idx,
        int32_t* assigned, ExceptionSink* xsink);

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

//! Add a native integer delta to a typed local via an AOT local slot
int64_t qore_rt_add_assign_local_int_aot(QoreAOTContext* ctx, int32_t idx,
        int64_t delta, ExceptionSink* xsink);

//! Increment a typed integer closure variable via an AOT local slot
int64_t qore_rt_increment_closure_int_aot(QoreAOTContext* ctx, int32_t idx,
        int64_t delta, ExceptionSink* xsink);

//! Invoke an expression via AOT context slot
uint64_t qore_rt_invoke_expr_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);

//! Return the raw QoreValue bits of an expression slot without evaluating
//! the node.  Used where the caller needs the AST pointer itself (e.g.
//! AOT-lowered `RefForeachInit` needs the `ParseReferenceNode*` to hand
//! to `qore_rt_ref_foreach_init`, which calls `evalToRef` internally).
uint64_t qore_rt_get_expr_bits_aot(QoreAOTContext* ctx, int32_t idx);

//! Construct value for VarRefNewObjectNode via AOT context slot (construct-only, no assignment)
uint64_t qore_rt_vrn_construct_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);

//! LValue load via AOT context slot
uint64_t qore_rt_lvalue_load_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);

//! LValue store via AOT context slot
uint64_t qore_rt_lvalue_store_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink);

//! LValue unary op via AOT context slot
uint64_t qore_rt_lvalue_unary_aot(int op, QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink);

//! LValue binary op via AOT context slot
uint64_t qore_rt_lvalue_binary_aot(int op, QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink);

//! LValue ternary op (splice) via AOT context slot
uint64_t qore_rt_lvalue_ternary_aot(int op, QoreAOTContext* ctx, int32_t idx, uint64_t first, uint64_t second,
    uint64_t third, ExceptionSink* xsink);

//! LValuePath runtime helpers — navigate structured lvalue path and execute operation
uint64_t qore_rt_lv_path_assign(QoreIRLValuePathInstruction* inst, uint64_t* dyn_vals,
    uint64_t rhs_bits, ExceptionSink* xsink);
uint64_t qore_rt_lv_path_compound(QoreIRLValuePathInstruction* inst, uint64_t* dyn_vals,
    uint64_t rhs_bits, ExceptionSink* xsink);
uint64_t qore_rt_lv_path_unary(QoreIRLValuePathInstruction* inst, uint64_t* dyn_vals,
    ExceptionSink* xsink);
uint64_t qore_rt_lv_path_binary_mut(QoreIRLValuePathInstruction* inst, uint64_t* dyn_vals,
    uint64_t rhs_bits, ExceptionSink* xsink);
uint64_t qore_rt_lv_path_ternary(QoreIRLValuePathInstruction* inst, uint64_t* dyn_vals,
    uint64_t a_bits, uint64_t b_bits, uint64_t c_bits, ExceptionSink* xsink);
uint64_t qore_rt_lv_path_assign_aot(QoreAOTContext* ctx, int32_t slot, uint64_t* dyn_vals,
    uint64_t rhs_bits, ExceptionSink* xsink);
uint64_t qore_rt_lv_path_compound_aot(QoreAOTContext* ctx, int32_t slot, uint64_t* dyn_vals,
    uint64_t rhs_bits, ExceptionSink* xsink);
uint64_t qore_rt_lv_path_unary_aot(QoreAOTContext* ctx, int32_t slot, uint64_t* dyn_vals,
    ExceptionSink* xsink);
uint64_t qore_rt_lv_path_binary_mut_aot(QoreAOTContext* ctx, int32_t slot, uint64_t* dyn_vals,
    uint64_t rhs_bits, ExceptionSink* xsink);
uint64_t qore_rt_lv_path_ternary_aot(QoreAOTContext* ctx, int32_t slot, uint64_t* dyn_vals,
    uint64_t a_bits, uint64_t b_bits, uint64_t c_bits, ExceptionSink* xsink);

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

//! AOT variant that also clears consumed caller temp cleanup slots after
//! callee parameter references have been established.
uint64_t qore_rt_call_with_args_aot_consume_args(QoreAOTContext* ctx, int32_t slot,
    uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink);

//! AOT fast direct call: resolve FunctionCallNode from context slot, extract function/variant/pgm,
//! then call qore_rt_call_fast() for fast function dispatch.
uint64_t qore_rt_call_direct_aot(QoreAOTContext* ctx, int32_t slot, uint64_t* args, int nargs,
    ExceptionSink* xsink);

//! Resolve the callee AOT context for a pre-resolved direct function call slot.
QoreAOTContext* qore_rt_get_aot_call_target_context(QoreAOTContext* ctx, int32_t slot,
    ExceptionSink* xsink);

//! Try to resolve the callee AOT context for a pre-resolved direct call slot.
//! Returns nullptr without raising when the target does not have cached AOT.
QoreAOTContext* qore_rt_try_get_aot_call_target_context(QoreAOTContext* ctx, int32_t slot);

//! Returns true when an assigned object receiver is still valid.
//! Used to guard receiver-independent AOT method fast entries.
int qore_rt_object_is_valid(uint64_t value);
//! Returns true when a valid object receiver has the exact class stored in an AOT call-target slot.
int qore_rt_object_has_exact_aot_target_class(QoreAOTContext* ctx, int32_t slot,
        uint64_t value);
//! Returns true when an object receiver has the exact class stored in an AOT call-target slot.
//! Unlike qore_rt_object_has_exact_aot_target_class(), this does not check object validity.
int qore_rt_object_has_exact_aot_target_class_only(QoreAOTContext* ctx, int32_t slot,
        uint64_t value);

//! AOT fast direct call with consumed caller temp cleanup slots.
uint64_t qore_rt_call_direct_aot_consume_args(QoreAOTContext* ctx, int32_t slot,
    uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink);

//! Direct function call — resolved at parse time, bypasses dynamic_cast chain and AST node copy.
//! Calls QoreFunction::evalFunctionTmpArgs() directly.
uint64_t qore_rt_call_function_direct(const QoreFunction* func, const AbstractQoreFunctionVariant* variant,
    QoreProgram* pgm, uint64_t* args, int nargs, ExceptionSink* xsink);

//! Fast function call — bypasses QoreListNode construction, CodeEvaluationHelper,
//! and the entire dispatch chain. Directly instantiates parameters from NaN-boxed args,
//! instantiates body locals, and calls the cached JIT/AOT function.
//! Same signature as qore_rt_call_function_direct so the same LLVM call site can
//! be used. Falls back to qore_rt_call_function_direct() if the callee is not JIT-compiled.
//! Returns NaN-boxed result; sets xsink on exception.
uint64_t qore_rt_call_fast(const QoreFunction* func, const AbstractQoreFunctionVariant* variant,
    QoreProgram* pgm, uint64_t* args, int nargs, ExceptionSink* xsink);

//! Constructor call with pre-evaluated NaN-boxed args.  Used by IR interpreter
//! and AOT LLVM codegen to avoid AST fallback — each constructor arg is
//! computed as a separate IR operand and passed as a NaN-boxed value.  execConstructor
//! and CodeEvaluationHelper handle default args and type coercion.
uint64_t qore_rt_new_object_nb(const QoreClass* qc,
    const AbstractQoreFunctionVariant* variant, const QoreTypeInfo* object_type_info,
    uint64_t* args, int nargs, ExceptionSink* xsink);

//! Constructor call variant that consumes caller-owned temporary argument cleanup slots
//! after the constructor argument list has taken any required references.
uint64_t qore_rt_new_object_nb_consume_args(const QoreClass* qc,
    const AbstractQoreFunctionVariant* variant, const QoreTypeInfo* object_type_info,
    uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink);

//! AOT variant of qore_rt_new_object_nb: loads qc/variant from the AOT context's
//! call_targets slot (populated at module load time from serialized class_path
//! + variant_sig).  Avoids baking stale class pointers into AOT native code.
uint64_t qore_rt_new_object_nb_aot(QoreAOTContext* ctx, int32_t slot,
    uint64_t* args, int nargs, ExceptionSink* xsink);

//! AOT constructor call variant that consumes caller-owned temporary argument cleanup slots.
uint64_t qore_rt_new_object_nb_aot_consume_args(QoreAOTContext* ctx,
    int32_t slot, uint64_t* args, uint64_t** arg_cleanups, int nargs,
    ExceptionSink* xsink);

//! Direct method call for devirtualized calls (final classes) — builds QoreListNode
//! and calls qore_method_private::eval().
uint64_t qore_rt_call_method_direct(const QoreMethod* method, uint64_t* args, int nargs,
    ExceptionSink* xsink);

//! Direct method call variant that consumes caller-owned temporary argument cleanup slots
//! after the callee has taken any required references.
uint64_t qore_rt_call_method_direct_consume_args(const QoreMethod* method, uint64_t* args,
    uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink);

//! Fast method call — bypasses QoreListNode construction for devirtualized method calls.
//! Directly instantiates parameters from NaN-boxed args and calls the cached JIT/AOT function.
//! Same signature as qore_rt_call_method_direct plus variant pointer.
//! Falls back to qore_rt_call_method_direct() if the callee is not JIT-compiled.
uint64_t qore_rt_call_method_fast(const QoreMethod* method, const AbstractQoreFunctionVariant* variant,
    uint64_t* args, int nargs, ExceptionSink* xsink);

//! Fast method call variant that consumes caller-owned temporary argument cleanup slots.
uint64_t qore_rt_call_method_fast_consume_args(const QoreMethod* method,
    const AbstractQoreFunctionVariant* variant, uint64_t* args, uint64_t** arg_cleanups,
    int nargs, ExceptionSink* xsink);

//! Fast call reference/closure call — takes the pre-evaluated call reference value and
//! pre-evaluated args (both NaN-boxed). Calls ResolvedCallReferenceNode::execValue() directly,
//! bypassing the dynamic_cast chain and AST node copy in qore_rt_call_with_args().
uint64_t qore_rt_call_ref_fast(uint64_t ref_bits, uint64_t* args, int nargs, ExceptionSink* xsink);

//! Fast function call with explicit target function pointer — used for multi-function LLVM
//! module compilation where the callee's native code address is known at compile time.
//! Same parameter setup as qore_rt_call_fast(), but calls the provided function pointer
//! directly instead of going through execCachedFunction(). Falls back to
//! qore_rt_call_function_direct() if the variant is not a user variant.
uint64_t qore_rt_call_fast_with_target(uint64_t (*target_fn)(ExceptionSink*),
    const AbstractQoreFunctionVariant* variant, uint64_t* args, int nargs, ExceptionSink* xsink);

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

//! Direct dot-eval method call with pre-evaluated base and arguments.
//! Handles object dispatch, class mismatch fallback, weak refs, and non-object pseudo dispatch.
//! base_bits is NaN-boxed base expression, args/nargs are pre-evaluated method arguments.
//! method/qc/variant are parse-time resolved pointers.
uint64_t qore_rt_dot_eval_method_direct(uint64_t base_bits, const QoreMethod* method, const QoreClass* qc,
    const AbstractQoreFunctionVariant* variant, uint64_t* args, int nargs, ExceptionSink* xsink);

//! Direct dot-eval method call with explicit generic method type arguments.
uint64_t qore_rt_dot_eval_method_direct_with_inst(uint64_t base_bits, const QoreMethod* method,
    const QoreClass* qc, const AbstractQoreFunctionVariant* variant, uint64_t* args, int nargs,
    const QoreTypeParamInstantiation* explicit_type_param_instantiation, ExceptionSink* xsink);

//! Direct dot-eval method call that consumes caller-owned temporary argument cleanup slots.
uint64_t qore_rt_dot_eval_method_direct_consume_args(uint64_t base_bits, const QoreMethod* method,
    const QoreClass* qc, const AbstractQoreFunctionVariant* variant, uint64_t* args,
    uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink);

//! Direct dot-eval method call with explicit type arguments that consumes caller-owned cleanup slots.
uint64_t qore_rt_dot_eval_method_direct_with_inst_consume_args(uint64_t base_bits, const QoreMethod* method,
    const QoreClass* qc, const AbstractQoreFunctionVariant* variant, uint64_t* args,
    uint64_t** arg_cleanups, int nargs, const QoreTypeParamInstantiation* explicit_type_param_instantiation,
    ExceptionSink* xsink);

//! Direct dot-eval pseudo-method call with pre-evaluated base and arguments.
//! Dispatches to qore_class_private::evalPseudoMethod() with QoreListNode built from args.
uint64_t qore_rt_dot_eval_pseudo_method_direct(uint64_t base_bits, const QoreMethod* method, const QoreClass* qc,
    const AbstractQoreFunctionVariant* variant, uint64_t* args, int nargs, ExceptionSink* xsink);

//! Direct dot-eval pseudo-method call with explicit generic type arguments for object/name fallback.
uint64_t qore_rt_dot_eval_pseudo_method_direct_with_inst(uint64_t base_bits, const QoreMethod* method,
    const QoreClass* qc, const AbstractQoreFunctionVariant* variant, uint64_t* args, int nargs,
    const QoreTypeParamInstantiation* explicit_type_param_instantiation, ExceptionSink* xsink);

//! Direct dot-eval pseudo-method call that consumes caller-owned temporary argument cleanup slots.
uint64_t qore_rt_dot_eval_pseudo_method_direct_consume_args(uint64_t base_bits, const QoreMethod* method,
    const QoreClass* qc, const AbstractQoreFunctionVariant* variant, uint64_t* args,
    uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink);

//! Direct dot-eval pseudo-method call with explicit type arguments that consumes cleanup slots.
uint64_t qore_rt_dot_eval_pseudo_method_direct_with_inst_consume_args(uint64_t base_bits,
    const QoreMethod* method, const QoreClass* qc, const AbstractQoreFunctionVariant* variant,
    uint64_t* args, uint64_t** arg_cleanups, int nargs,
    const QoreTypeParamInstantiation* explicit_type_param_instantiation, ExceptionSink* xsink);

//! Guarded fast path for no-arg <list>::empty() and <list>::val() pseudo-methods.
//! Fast path handles list and NOTHING; all other values fall back to generic pseudo dispatch.
uint64_t qore_rt_pseudo_list_bool_guarded(uint64_t base_bits, const QoreMethod* method, const QoreClass* qc,
    const AbstractQoreFunctionVariant* variant, int32_t invert_empty, ExceptionSink* xsink);

//! Guarded fast path for no-arg <list>::first() and <list>::last() pseudo-methods.
//! Fast path handles list; all other values fall back to generic pseudo dispatch.
uint64_t qore_rt_pseudo_list_value_guarded(uint64_t base_bits, const QoreMethod* method, const QoreClass* qc,
    const AbstractQoreFunctionVariant* variant, int32_t last, ExceptionSink* xsink);

//! Fast no-guard pseudo-methods: <string>::startsWith()/endsWith()/contains() for assigned string operands.
uint64_t qore_rt_pseudo_string_predicate_noguard(uint64_t val_bits, uint64_t arg_bits, int32_t predicate,
    ExceptionSink* xsink);

//! Native scalar variant of qore_rt_pseudo_string_predicate_noguard().
int64_t qore_rt_pseudo_string_predicate_native_noguard(uint64_t val_bits, uint64_t arg_bits,
    int32_t predicate, ExceptionSink* xsink);

//! Fast no-guard pseudo-method: <string>::find() for assigned string base and substring operands.
uint64_t qore_rt_pseudo_string_find_noguard(uint64_t val_bits, uint64_t substring_bits, int64_t offset,
    ExceptionSink* xsink);

//! Native scalar variant of qore_rt_pseudo_string_find_noguard().
int64_t qore_rt_pseudo_string_find_native_noguard(uint64_t val_bits, uint64_t substring_bits,
    int64_t offset, ExceptionSink* xsink);

//! Fast no-guard pseudo-method: <string>::rfind() for assigned string base and substring operands.
uint64_t qore_rt_pseudo_string_rfind_noguard(uint64_t val_bits, uint64_t substring_bits, int64_t offset,
    ExceptionSink* xsink);

//! Native scalar variant of qore_rt_pseudo_string_rfind_noguard().
int64_t qore_rt_pseudo_string_rfind_native_noguard(uint64_t val_bits, uint64_t substring_bits,
    int64_t offset, ExceptionSink* xsink);

//! Native scalar <string>::size()/strlen() for bases proven to be assigned strings.
int64_t qore_rt_pseudo_string_size_native_noguard(uint64_t val_bits);

//! Native scalar <string>::length() for bases proven to be assigned strings.
int64_t qore_rt_pseudo_string_length_native_noguard(uint64_t val_bits);

//! Fast no-guard pseudo-method: <string>::substr() for assigned string base and int operands.
uint64_t qore_rt_pseudo_string_substr_noguard(uint64_t val_bits, int64_t start, int64_t length,
    int32_t has_length, ExceptionSink* xsink);

//! Fast no-guard pseudo-method: <string>::lwr() for assigned string bases.
uint64_t qore_rt_pseudo_string_lwr_noguard(uint64_t val_bits, ExceptionSink* xsink);

//! Fast no-guard pseudo-method: <string>::upr() for assigned string bases.
uint64_t qore_rt_pseudo_string_upr_noguard(uint64_t val_bits, ExceptionSink* xsink);

//! Fast no-guard pseudo-method: <string>::toInt() for assigned string bases.
uint64_t qore_rt_pseudo_string_to_int_noguard(uint64_t val_bits, ExceptionSink* xsink);

//! Fast pseudo-method helper: type()/typename() returns the runtime type name.
uint64_t qore_rt_pseudo_type(uint64_t val_bits);

//! Fast pseudo-method helper: <value>::toNumber().
uint64_t qore_rt_pseudo_toNumber(uint64_t val_bits);

//! Name-based dot-eval method call that consumes caller-owned temporary argument cleanup slots.
uint64_t qore_rt_dot_eval_method_by_name_consume_args(uint64_t base_bits, const char* method_name,
    uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink);

//! Name-based dot-eval method call with explicit generic method type arguments.
uint64_t qore_rt_dot_eval_method_by_name_with_inst(uint64_t base_bits, const char* method_name,
    uint64_t* args, int nargs, const QoreTypeParamInstantiation* explicit_type_param_instantiation,
    ExceptionSink* xsink);

//! Name-based dot-eval method call with explicit type arguments that consumes cleanup slots.
uint64_t qore_rt_dot_eval_method_by_name_with_inst_consume_args(uint64_t base_bits, const char* method_name,
    uint64_t* args, uint64_t** arg_cleanups, int nargs,
    const QoreTypeParamInstantiation* explicit_type_param_instantiation, ExceptionSink* xsink);

//! AOT variant of qore_rt_dot_eval_method_direct: resolves method/qc/variant from context slot.
uint64_t qore_rt_dot_eval_method_direct_aot(QoreAOTContext* ctx, int32_t slot, uint64_t base_bits,
    uint64_t* args, int nargs, ExceptionSink* xsink);

//! AOT dot-eval path for an assigned object receiver; falls back if the runtime tag differs.
uint64_t qore_rt_dot_eval_object_method_direct_aot(QoreAOTContext* ctx, int32_t slot,
    uint64_t base_bits, uint64_t* args, int nargs, ExceptionSink* xsink);

//! Load an exact final-class getter member, falling back to normal method dispatch when guarded checks fail.
uint64_t qore_rt_load_object_getter_aot(QoreAOTContext* ctx, int32_t slot,
    uint64_t base_bits, const char* member_name, ExceptionSink* xsink);

//! Getter variant carrying the compile-time declared-return NOTHING contract.
uint64_t qore_rt_load_object_getter_checked_aot(QoreAOTContext* ctx, int32_t slot,
    uint64_t base_bits, const char* member_name, int32_t rejects_nothing,
    ExceptionSink* xsink);

//! AOT variant that consumes caller-owned temporary argument cleanup slots.
uint64_t qore_rt_dot_eval_method_direct_aot_consume_args(QoreAOTContext* ctx, int32_t slot,
    uint64_t base_bits, uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink);

//! AOT variant of qore_rt_dot_eval_pseudo_method_direct: resolves from context slot.
uint64_t qore_rt_dot_eval_pseudo_method_direct_aot(QoreAOTContext* ctx, int32_t slot, uint64_t base_bits,
    uint64_t* args, int nargs, ExceptionSink* xsink);

//! AOT pseudo-method variant that consumes caller-owned temporary argument cleanup slots.
uint64_t qore_rt_dot_eval_pseudo_method_direct_aot_consume_args(QoreAOTContext* ctx, int32_t slot,
    uint64_t base_bits, uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink);

//! AOT guarded fast path for no-arg <list>::empty() and <list>::val() pseudo-methods.
uint64_t qore_rt_pseudo_list_bool_guarded_aot(QoreAOTContext* ctx, int32_t slot, uint64_t base_bits,
    int32_t invert_empty, ExceptionSink* xsink);

//! AOT guarded fast path for no-arg <list>::first() and <list>::last() pseudo-methods.
uint64_t qore_rt_pseudo_list_value_guarded_aot(QoreAOTContext* ctx, int32_t slot, uint64_t base_bits,
    int32_t last, ExceptionSink* xsink);

//! Direct static method call with pre-evaluated arguments — builds QoreListNode.
//! Calls qore_method_private::eval() with nullptr for self.
uint64_t qore_rt_call_static_method_direct(const QoreMethod* method,
    const AbstractQoreFunctionVariant* variant, uint64_t* args, int nargs, ExceptionSink* xsink);

//! AOT variant of qore_rt_call_static_method_direct: resolves method from context slot.
uint64_t qore_rt_call_static_method_direct_aot(QoreAOTContext* ctx, int32_t slot, uint64_t* args,
    int nargs, ExceptionSink* xsink);

//! AOT static method call with consumed caller temp cleanup slots.
uint64_t qore_rt_call_static_method_direct_aot_consume_args(QoreAOTContext* ctx, int32_t slot,
    uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink);

//! AOT direct method call with consumed caller temp cleanup slots.
uint64_t qore_rt_call_method_direct_aot_consume_args(QoreAOTContext* ctx, int32_t slot,
    uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink);

//! AOT fast method call with consumed caller temp cleanup slots.
uint64_t qore_rt_call_method_fast_aot_consume_args(QoreAOTContext* ctx, int32_t slot,
    uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink);

//! Cast operation with pre-evaluated inner value.
//! cast_expr_bits is the NaN-boxed cast operator AST node (QoreCastOperatorNode*).
//! inner_bits is the NaN-boxed pre-evaluated inner value to cast.
//! Returns the NaN-boxed cast result; sets xsink on RUNTIME-CAST-ERROR.
uint64_t qore_rt_cast_with_inner(uint64_t cast_expr_bits, uint64_t inner_bits, ExceptionSink* xsink);

//! AOT variant: resolve cast expression from context slot, then delegate to qore_rt_cast_with_inner
uint64_t qore_rt_cast_with_inner_aot(QoreAOTContext* ctx, int32_t slot, uint64_t inner_bits,
    ExceptionSink* xsink);

//! Cast using a serialized type path, resolving names against the current runtime program.
uint64_t qore_rt_cast_by_type_path(uint64_t inner_bits, const char* type_path, int64_t or_nothing,
    ExceptionSink* xsink);

//! AOT variant: resolve the type path against the AOT context's owning program.
uint64_t qore_rt_cast_by_type_path_aot(QoreAOTContext* ctx, uint64_t inner_bits, const char* type_path,
    int64_t or_nothing, ExceptionSink* xsink);

uint64_t qore_rt_cast_by_type_path_throwing(uint64_t inner_bits, const char* type_path, int64_t or_nothing,
    ExceptionSink* xsink);
uint64_t qore_rt_cast_by_type_path_aot_throwing(QoreAOTContext* ctx, uint64_t inner_bits,
    const char* type_path, int64_t or_nothing, ExceptionSink* xsink);

//! Switch case match: calls CaseNode::matches() which unwraps TAG_ENUM before isEqualHard()
//! \param case_node_ptr pointer to the CaseNode (cast to void* for C ABI)
//! \param switch_val_bits NaN-boxed switch value
//! \param xsink exception sink
//! \returns NaN-boxed bool result
uint64_t qore_rt_switch_case_match(const void* case_node_ptr, uint64_t switch_val_bits,
    ExceptionSink* xsink);

//! Get pointer to TLS slot cache variable for threading parent scope to exception-path handlers
//! Phase 2, Fix 2a: Used by QoreIRInterpreter to set/restore the current IR frame's slot cache
//! Returns address of thread-local pointer (cast to void** for C ABI compatibility)
void** qore_rt_get_ir_slot_cache_ptr();

//! Get pointer to the TLS dirty bitmap associated with a native IR slot cache.
//! Returns address of thread-local pointer (cast to void** for C ABI compatibility).
void** qore_rt_get_ir_slot_cache_dirty_ptr();

//! Call a closure/call reference with 0 arguments
uint64_t qore_rt_call_closure_0(uint64_t ref_bits, ExceptionSink* xsink);

//! Call a closure/call reference with 1 argument
uint64_t qore_rt_call_closure_1(uint64_t ref_bits, uint64_t arg0_bits, ExceptionSink* xsink);

//! Call a closure/call reference with N arguments (fast path, pre-evaluated args)
uint64_t qore_rt_call_closure_fast(uint64_t ref_bits, uint64_t* args, int nargs, ExceptionSink* xsink);

//! Call a closure/call reference with N arguments and consume caller-owned argument cleanup refs.
uint64_t qore_rt_call_closure_fast_consume_args(uint64_t ref_bits, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink);

} // extern "C"

#endif

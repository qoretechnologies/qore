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
void qore_rt_uninstantiate_local(ExceptionSink* xsink);

} // extern "C"

#endif

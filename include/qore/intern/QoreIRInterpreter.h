/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRInterpreter.h

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

#ifndef _QORE_INTERN_QOREIRINTERPRETER_H
#define _QORE_INTERN_QOREIRINTERPRETER_H

#include <qore/intern/QoreIR.h>

#include <string>
#include <unordered_set>
#include <vector>

class ExceptionSink;
class LocalVar;
class QoreProgram;
class QoreValue;
class QoreClosureBase;
class AbstractStatement;
class StatementBlock;
struct AOTScalarLeafInfo;

//! Direct params for IR-to-IR calls — bypasses TLS variable stack entirely.
//! Param values are placed directly into the IR slot cache, eliminating
//! the TLS push/pop round-trip (instantiate → eval → uninstantiate).
struct IRDirectParams {
    IRDirectParams(const uint64_t* args, int nargs, uint64_t** arg_cleanups = nullptr)
        : args(args), arg_cleanups(arg_cleanups), nargs(nargs) {
    }

    const uint64_t* args;       //!< NaN-boxed param values
    uint64_t** arg_cleanups;    //!< caller temp cleanup slots to clear after param refs are taken
    int nargs;                  //!< number of params
};

class QoreIRInterpreter {
public:
    static QoreValue evalComparison(QoreIROpcode op, const QoreValue& left, const QoreValue& right,
            ExceptionSink* xsink);
    //! Evaluates a plugin comparison (QoreIROpcode::EqPlugin...GePlugin)
    /** dispatches to the plugin operation for such a value, as the AST operator does, and evaluates the comparison
        otherwise (qore#5911)
    */
    static QoreValue evalPluginComparison(QoreIROpcode op, const QoreValue& left, const QoreValue& right,
            ExceptionSink* xsink);
    static QoreValue evalExpr(QoreIROpcode op, const QoreValue& expr, ExceptionSink* xsink);
    static QoreValue evalUnary(QoreIROpcode op, const QoreValue& value, ExceptionSink* xsink);
    static QoreValue evalBinary(QoreIROpcode op, const QoreValue& left, const QoreValue& right,
            ExceptionSink* xsink);
    static QoreValue evalTernary(QoreIROpcode op, const QoreValue& first, const QoreValue& second,
            const QoreValue& third, ExceptionSink* xsink);
    static QoreValue evalQuaternary(QoreIROpcode op, const QoreValue& first, const QoreValue& second,
            const QoreValue& third, const QoreValue& fourth, ExceptionSink* xsink);
    static QoreValue evalLValueLoad(const QoreValue& lvalue, ExceptionSink* xsink);
    static QoreValue evalLValueStore(const QoreValue& lvalue, const QoreValue& value, ExceptionSink* xsink,
            AssignmentMode mode = AssignmentMode::Normal);
    static QoreValue evalLValueUnary(QoreIROpcode op, const QoreValue& lvalue, ExceptionSink* xsink);
    static QoreValue evalLValueBinary(QoreIROpcode op, const QoreValue& lvalue, const QoreValue& right,
            ExceptionSink* xsink);
    static QoreValue evalLValueTernary(QoreIROpcode op, const QoreValue& lvalue, const QoreValue& first,
            const QoreValue& second, const QoreValue& third, ExceptionSink* xsink);
    static bool simulateInvoke(QoreIROpcode op, const QoreValue& expr, ExceptionSink* xsink);
    static int execStatement(QoreIROpcode op, const AbstractStatement* stmt, QoreValue& return_value,
            ExceptionSink* xsink);
    //! Runs an IR function
    /** @param statements the block of the function, which the debugger's events are given with; with \a pgm, the
        frame makes the debugger's events while a debugger is attached
        @param handler_frame true for the code of an on_exit, on_error or on_success handler, which makes the
        debugger's events of its statements but not those of a function (enter and exit)
    */
    static bool execute(const QoreIRFunction& func, QoreValue& return_value, ExceptionSink* xsink,
            std::vector<std::string>* cleanup_log = nullptr, const std::vector<QoreValue>* args = nullptr,
            const std::vector<QoreValue>* closure = nullptr,
            const std::unordered_set<const LocalVar*>* pre_instantiated = nullptr,
            const LocalVar* excluded_selfid = nullptr,
            const StatementBlock* statements = nullptr, QoreProgram* pgm = nullptr,
            bool suppress_guard_deopt = false,
            const IRDirectParams* direct_params = nullptr,
            std::vector<QoreValue>* parent_slot_cache = nullptr, bool handler_frame = false);

    //! Emit a `[ir-silent-fail:<tag>]` line to stderr naming the last IR
    //! opcode + source location that ran before `execute()` returned
    //! `ok=false` without raising an xsink exception.  Gated on
    //! QORE_IR_TRACE_SILENT_FAIL=1 at the caller.  Thread-local state is
    //! populated by execute()'s instruction dispatch loop; it remains
    //! valid until the next execute() call on this thread.
    DLLLOCAL static void dumpLastSilentFail(const char* tag);
};

//! Returns the number of entries on the innermost running IR-interpreter frame's temp cleanup vector on this
//! thread, or 0 when no IR frame is running.  Debug-only introspection (dbg_get_ir_cleanup_entries()): a break or
//! continue that left a temp scope without draining it used to leave a sentinel on this vector every time it ran,
//! so a test reads this inside a loop to prove the vector no longer grows.  See design/ir-loop-jump-temp-scope.md.
DLLLOCAL int64 q_get_ir_innermost_cleanup_entries();

//! Returns the number of entries in the innermost running IR-interpreter frame's list of the locals it has
//! instantiated on this thread, or 0 when no IR frame is running.  Debug-only introspection
//! (dbg_get_ir_instantiated_local_entries()): the list used to grow by every local that a loop body instantiated in
//! every iteration, and a continue that uninstantiated a local the iteration had not instantiated scanned all of
//! it, so a test reads this inside a loop to prove the list no longer grows.
DLLLOCAL int64 q_get_ir_innermost_instantiated_local_entries();

//! Compound +=/-= helpers — shared between IR interpreter and JIT runtime
/** The value added holds a reference in \a right, which += takes when it stores the value - appended to a list or
    assigned to an lvalue with no value - as the AST interpreter does; a caller that does not use the value again
    can therefore hand its own reference over, so that no dereference follows once the value is linked into a graph
    (see design/dgc.md, "The rrefs deferral").  The holder must be declared before the LValueHelper, so that a
    reference that += does not take is released after the lvalue's locks.
*/
DLLLOCAL QoreValue doPlusEqualsOnLValue(LValueHelper& v, ValueHolder& right, ExceptionSink* xsink);
DLLLOCAL QoreValue doPlusEqualsOnLValue(LValueHelper& v, ValueHolder& right, ExceptionSink* xsink,
    LexicalSemanticPolicy policy);
DLLLOCAL QoreValue doMinusEqualsOnLValue(LValueHelper& v, const QoreValue& right, ExceptionSink* xsink);

//! Applies a step of a list of selectors for the IR interpreter (QoreIROpcode::ListSelectorStep)
/** As qore_rt_list_selector_step(), but the selector values are read from the IR value slots \a slots, at the
    \a count value IDs \a selector_ids, instead of from an array, which would be in the frame of
    QoreIRInterpreter::execute() and so in the frame of every interpreted call
*/
DLLLOCAL uint64_t qore_ir_list_selector_step(uint64_t result_val, uint64_t seq_val, int64_t flags,
        const QoreValue* slots, const QoreIRValue* selector_ids, int32_t count, ExceptionSink* xsink);

//! Applies a compound assignment to an lvalue as the AST interpreter does
/** The value holds a reference in \a rhs_holder, which += takes when it stores the value (see
    doPlusEqualsOnLValue()); the holder must be declared before the LValueHelper.

    @return the new value of the lvalue, holding a reference, or no value
*/
DLLLOCAL QoreValue qore_rt_apply_lvalue_compound(LValueHelper& lvh, LVCompoundOp compound_op,
        ValueHolder& rhs_holder, ExceptionSink* xsink);

//! Returns true if += of \a right to the lvalue of \a lvh can store a value that needs a reference element bound
/** A container whose element or member types can bind a reference, or an lvalue with no value whose type can, as for
    the += operator of the AST interpreter: the values added are then checked with a probe first (qore#7391)
*/
DLLLOCAL bool qore_plus_equals_may_bind(LValueHelper& lvh, const QoreValue& right);

//! Applies a compound assignment to the lvalue that \a resolve resolves, binding reference elements outside its locks
/** As qore_rt_apply_lvalue_compound(), with the LValueHelper made here: += that stores values that need a reference
    element bound (appended to a container whose element or member types can bind a reference, or assigned to an
    lvalue with no value whose type can) is made again after they have been bound with no lvalue lock held, and the
    target is resolved again with \a resolve for each round, which evaluates nothing again (qore#7391)

    @param compound_op the operation
    @param rhs_holder the value, see qore_rt_apply_lvalue_compound()
    @param xsink for exceptions
    @param resolve resolves the target in the helper given; returns non-zero if it cannot be
    @param resolve_arg the argument of \a resolve
    @param resolve_failed set to true if \a resolve failed

    @return the new value of the lvalue, holding a reference, or no value
*/
DLLLOCAL QoreValue qore_rt_lvalue_compound_with_bind_retry(LVCompoundOp compound_op, ValueHolder& rhs_holder,
        ExceptionSink* xsink, int (*resolve)(LValueHelper& lvh, void* arg), void* resolve_arg,
        bool& resolve_failed);

//! Applies push or unshift to the lvalue at \a path as the AST interpreter does
/** An lvalue with no value whose type accepts a list is assigned an empty list first.  A value that needs a reference
    element bound for the element type of the list is added after it has been bound with no lvalue lock held: the path
    is resolved again with the keys evaluated already (qore#7391).

    @param path the path, with its keys evaluated
    @param push true for push, false for unshift
    @param rhs_holder holds a reference for the value added, which the list takes; it must be declared before any
    LValueHelper of the caller
    @param want_result true to return the list
    @param xsink for exceptions
    @param navigation_failed set to true if the path could not be resolved

    @return the list with a reference for the caller if \a want_result is true and no exception was raised, otherwise
    no value
*/
DLLLOCAL QoreValue qore_rt_lv_path_push_unshift(const IRLVPath& path, bool push, ValueHolder& rhs_holder,
        bool want_result, ExceptionSink* xsink, bool& navigation_failed);

//! Replaces elements of the list \a vl, which \a lvh holds, as qore_list_private::replace() does (splice, extract)
/** A list whose element type can bind a reference has the values of \a replacement checked with a probe first: if a
    reference element needs a bind, the list is not changed, and the caller leaves its helper and makes the operation
    again after QoreBindRetry::again() has bound it (qore#7391)

    @param rv the list that replace() returns, owned by the caller, or nullptr

    @return -1 if the list was not changed (the operation is made again, or an exception was raised), 0 if not
*/
DLLLOCAL int qore_rt_list_replace_with_bind_probe(LValueHelper& lvh, QoreListNode& vl, size_t offset, size_t length,
        const QoreValue& replacement, bool extract, QoreListNode*& rv, ExceptionSink* xsink);

//! Applies ++ or -- (LVUnaryOp::PreInc, PreDec, PostInc or PostDec) to an lvalue as the AST interpreter does
/** The operation follows the type of the value, as QorePreIncrementOperatorNode::evalImpl() and the other operators
    do, and an error that the lvalue's type raises names the operator as they do (ex: <tt><++ (pre) operator></tt>
    for an enum lvalue, which refuses the plain value computed, qore#6197).  The operation is a template argument and
    the function is inlined in a case of the caller's switch over the operations (the IR interpreter and the runtime
    helpers of native code); it reads no exception sink, so that ++ and -- cost what they cost before they named the
    operator.

    @tparam Op the operation: LVUnaryOp::PreInc, PreDec, PostInc or PostDec

    @param lvh the lvalue
    @param ref_rv false if the value of a post operation is not used: the old value of a number is not copied
    @param res set to the value of the operation (the new value, or the old value for a post operator), holding a
    reference; after an exception, the caller must release it (it can be the value that the lvalue kept)
*/
template <LVUnaryOp Op>
QORE_INTERN_ALWAYS_INLINE void qore_rt_apply_lvalue_incdec(LValueHelper& lvh, bool ref_rv, QoreValue& res) {
    static_assert(Op == LVUnaryOp::PreInc || Op == LVUnaryOp::PreDec || Op == LVUnaryOp::PostInc
        || Op == LVUnaryOp::PostDec, "not an increment or decrement");
    // the descriptions are those of the AST operators (QorePreIncrementOperatorNode::evalImpl() and the others)
    constexpr const char* desc = Op == LVUnaryOp::PreInc ? "<++ (pre) operator>"
        : (Op == LVUnaryOp::PreDec ? "<-- (pre) operator>"
        : (Op == LVUnaryOp::PostInc ? "<++ (post) operator>" : "<-- (post) operator>"));
    qore_type_t t = lvh.getType();
    if (t == NT_NUMBER) {
        if constexpr (Op == LVUnaryOp::PreInc) {
            lvh.preIncrementNumber(desc);
            res = lvh.getReferencedValue();
        } else if constexpr (Op == LVUnaryOp::PreDec) {
            lvh.preDecrementNumber(desc);
            res = lvh.getReferencedValue();
        } else {
            // no value if the lvalue's type refused the result
            QoreNumberNode* n = Op == LVUnaryOp::PostInc ? lvh.postIncrementNumber(ref_rv, desc)
                : lvh.postDecrementNumber(ref_rv, desc);
            if (n) {
                res = n;
            }
        }
    } else if (t == NT_FLOAT) {
        if constexpr (Op == LVUnaryOp::PreInc) {
            res = lvh.preIncrementFloat(desc);
        } else if constexpr (Op == LVUnaryOp::PreDec) {
            res = lvh.preDecrementFloat(desc);
        } else if constexpr (Op == LVUnaryOp::PostInc) {
            res = lvh.postIncrementFloat(desc);
        } else {
            res = lvh.postDecrementFloat(desc);
        }
    } else {
        if constexpr (Op == LVUnaryOp::PreInc) {
            res = lvh.preIncrementBigInt(desc);
        } else if constexpr (Op == LVUnaryOp::PreDec) {
            res = lvh.preDecrementBigInt(desc);
        } else if constexpr (Op == LVUnaryOp::PostInc) {
            res = lvh.postIncrementBigInt(desc);
        } else {
            res = lvh.postDecrementBigInt(desc);
        }
    }
}

//! Returns true if the lvalue type is an int type (int, *int, softint), but not timeout
DLLLOCAL bool qore_is_int_lvalue_type(const QoreTypeInfo* ti);

//! Applies -= with a date operand to an lvalue of an int type
/** The lvalue is assigned the result of the binary operator, as <tt>x = x - d</tt> assigns it: the result is a
    date, which an int lvalue does not accept (RUNTIME-TYPE-ERROR) and a softint lvalue converts

    @return the new value of the lvalue, holding a reference, or no value if an exception was raised
*/
DLLLOCAL QoreValue qore_int_lvalue_minus_date(LValueHelper& v, const QoreValue& right, ExceptionSink* xsink);

//! Returns the value that a compound assignment gives an untyped lvalue holding the value given
/** The result is the one the AST interpreter gives: the operation follows the type of the current value (ex: an
    int value converts the operand to an int, so 10 += 1.5 gives 11).  It serves the opcodes that load a variable,
    compute its new value and store it; a variable whose declared type changes the operation (ex: \c timeout) is
    changed through its lvalue instead.  The value is computed for an lvalue of type \c auto, whose assignment
    converts nothing, so a value that the operation assigns keeps its complex type (ex: \c += on an lvalue with no
    value gives the operand itself, and a \c hash<HD> stays a \c hash<HD>); an lvalue without any type would strip
    it.  Variables declared \c any or \c auto! are changed through this function too: the store of the new value
    applies their declared type, which strips the complex type as their assignment does.

    @param op the compound operation
    @param left the current value of the lvalue
    @param right the operand
    @param xsink for exceptions raised by the operation

    @return the new value of the lvalue, holding a reference, or no value if an exception was raised
*/
DLLLOCAL QoreValue qore_compound_assign_value(LVCompoundOp op, const QoreValue& left, const QoreValue& right,
        ExceptionSink* xsink);

//! Execute a previously resolved direct-call descriptor when its cached IR is
//! a supported native scalar leaf. Returns false without side effects when the
//! body or runtime argument types require normal call semantics.
DLLLOCAL bool qore_ir_try_execute_native_leaf(QoreIRCallDirectInstruction* inst,
        uint64_t* args, int nargs, QoreValue& result,
        const QoreClosureBase* closure = nullptr);
//! Return true when the IR body is covered by the direct native leaf executor.
DLLLOCAL bool qore_ir_is_native_leaf(const QoreIRFunction* ir,
        const UserVariantBase* uvb, int nargs);
//! Return an importable pure scalar leaf descriptor for cross-object AOT lowering.
DLLLOCAL bool qore_ir_get_aot_scalar_leaf(const QoreIRFunction* ir,
        const UserVariantBase* uvb, int nargs, AOTScalarLeafInfo& result);

//! Holds an IR value that an operation reads, evaluating a value that needs evaluation except a reference
/** The AST interpreter gives an operation the value of its operand expression as it is: an expression giving a
    reference (ex: an element <tt>h.r</tt> holding <tt>\x</tt>) gives the reference itself, which a container access
    or an operator does not read (ex: <tt>h.r[0]</tt> and <tt>h.r.a</tt> are \c NOTHING).  An IR value is the value
    of such an expression already, so a reference is held as it is instead of being evaluated, which would read the
    referenced lvalue.
*/
class QoreIROperandHolder : public ValueEvalOptimizedRefHolder {
public:
    DLLLOCAL QoreIROperandHolder(const QoreValue& value, ExceptionSink* xs) : ValueEvalOptimizedRefHolder(xs) {
        if (value.getType() == NT_REFERENCE) {
            setValue(value, false);
        } else {
            eval(value);
        }
    }
};

#ifdef DEBUG
//! the closure calls that found a cache entry made for a closure of a deleted Program at the same addresses
DLLLOCAL extern std::atomic<int64_t> qore_dbg_closure_call_cache_program_misses;
#endif

#endif

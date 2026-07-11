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
class AbstractStatement;
class StatementBlock;

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
            bool weak = false);
    static QoreValue evalLValueUnary(QoreIROpcode op, const QoreValue& lvalue, ExceptionSink* xsink);
    static QoreValue evalLValueBinary(QoreIROpcode op, const QoreValue& lvalue, const QoreValue& right,
            ExceptionSink* xsink);
    static QoreValue evalLValueTernary(QoreIROpcode op, const QoreValue& lvalue, const QoreValue& first,
            const QoreValue& second, const QoreValue& third, ExceptionSink* xsink);
    static bool simulateInvoke(QoreIROpcode op, const QoreValue& expr, ExceptionSink* xsink);
    static int execStatement(QoreIROpcode op, const AbstractStatement* stmt, QoreValue& return_value,
            ExceptionSink* xsink);
    static bool execute(const QoreIRFunction& func, QoreValue& return_value, ExceptionSink* xsink,
            std::vector<std::string>* cleanup_log = nullptr, const std::vector<QoreValue>* args = nullptr,
            const std::vector<QoreValue>* closure = nullptr,
            const std::unordered_set<const LocalVar*>* pre_instantiated = nullptr,
            const LocalVar* excluded_selfid = nullptr,
            const StatementBlock* statements = nullptr, QoreProgram* pgm = nullptr,
            bool suppress_guard_deopt = false,
            const IRDirectParams* direct_params = nullptr,
            std::vector<QoreValue>* parent_slot_cache = nullptr);

    //! Emit a `[ir-silent-fail:<tag>]` line to stderr naming the last IR
    //! opcode + source location that ran before `execute()` returned
    //! `ok=false` without raising an xsink exception.  Gated on
    //! QORE_IR_TRACE_SILENT_FAIL=1 at the caller.  Thread-local state is
    //! populated by execute()'s instruction dispatch loop; it remains
    //! valid until the next execute() call on this thread.
    DLLLOCAL static void dumpLastSilentFail(const char* tag);
};

//! Compound +=/-= helpers — shared between IR interpreter and JIT runtime
DLLLOCAL QoreValue doPlusEqualsOnLValue(LValueHelper& v, const QoreValue& right, ExceptionSink* xsink);
DLLLOCAL QoreValue doMinusEqualsOnLValue(LValueHelper& v, const QoreValue& right, ExceptionSink* xsink);

//! Execute a previously resolved direct-call descriptor when its cached IR is
//! a supported native scalar leaf. Returns false without side effects when the
//! body or runtime argument types require normal call semantics.
DLLLOCAL bool qore_ir_try_execute_native_leaf(QoreIRCallDirectInstruction* inst,
        uint64_t* args, int nargs, QoreValue& result);
//! Return true when the IR body is covered by the direct native leaf executor.
DLLLOCAL bool qore_ir_is_native_leaf(const QoreIRFunction* ir,
        const UserVariantBase* uvb, int nargs);

#endif

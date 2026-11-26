/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAOTStringRuntime.cpp

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

// The calls of the canonical <string>::split(string) and trim(string) builtins that AOT code makes without an argument
// list (issue #5578, see design/aot-string-split-trim.md), and the ordinary calls of the string builtins that the fast
// paths make to raise their exceptions in the builtin's frame (qore#7711, QoreStringFastPathCalls.h).  Compiled in a
// unit of its own, outside the single compilation unit: in that unit, the code below would change how the compiler
// inlines the call helpers that it shares with the other calls (CodeContextHelper, the stack location updates), and so
// the code of those calls.

#include "qore/intern/QoreJITIncludes.h"
#include "qore/intern/BuiltinFunction.h"
#include "qore/intern/FastRouteGuard.h"
#include "qore/intern/QorePseudoMethods.h"
#include "qore/intern/QoreStringFastPathCalls.h"
#include "qore/intern/RuntimeConfig.h"
#include "qore/intern/qore_thread_intern.h"

#include <atomic>
#include <string>

// the direct call mode of the AOT calls (see JITRuntime.cpp): 2 or more while a Program has a debugger, and its low
// bit is set until a call has checked QORE_AOT_DISABLE_PRELINKED_CALLS (qore_aot_prelinked_override(), which clears
// it unless the variable is set)
extern DLLLOCAL std::atomic<unsigned> qore_aot_direct_call_mode;
DLLLOCAL bool qore_aot_prelinked_override(unsigned mode);

namespace {
//! true if a call must take the ordinary path: while a Program has a debugger, or if QORE_AOT_DISABLE_PRELINKED_CALLS
//! is set; checks the variable as the ordinary direct call does, so the first call clears the pending check
inline bool qore_aot_string_call_needs_ordinary_path() {
    unsigned mode = qore_aot_direct_call_mode.load(std::memory_order_acquire);
    return mode && (mode >= 2 || qore_aot_prelinked_override(mode));
}
}

namespace {
//! The frame of a builtin call that the AOT code makes without an argument list
/** It does what CodeEvaluationHelper does for a call of a builtin variant that was resolved when the code was
    compiled and that has no receiver, no type parameters and no defaults to evaluate (the omitted arguments of the
    canonical variants are constants that the code passes itself), in the same order: it takes the pending call site
    and the call reference's mark, checks the stack, takes the caller's parse options, enters the call's Program
    (\a pgm_ctx: the Program of a function call, nullptr for a pseudo-method), switches the runtime parse options to
    the Program's, sets the return type and the variant's runtime flags, and pushes the builtin's stack location,
    which an exception records; the destructor restores all of it in the reverse order.

    A call of a function enters its Program only if it is the thread's current Program: the AOT helpers below take
    the ordinary path otherwise, as for a module call, which needs the runtime parse options override of the
    crossing.
*/
class QoreAOTBuiltinCallFrame final : public QoreStackLocation {
public:
    QoreAOTBuiltinCallFrame(RuntimeConfig& rc, const std::string& call_name,
            const AbstractQoreFunctionVariant& variant, QoreProgram* pgm_ctx, ExceptionSink* xsink)
            : rc(rc), call_name(call_name), loc(take_runtime_call_location()) {
        // Qore code must never be started on an async I/O thread answering a fast route
        QORE_ASSERT_NOT_IN_FAST_ROUTE();
        assert(&rc == &rc_get_tls_ref());
        assert(!variant.isUser());
        // the mark of a call through a call reference is for the call that it makes, not for this one
        rc_take_call_reference_call(rc, nullptr);
#ifdef QORE_MANAGE_STACK
        if (check_stack(xsink)) {
            return;
        }
#endif
        // the first call made in the scope of a QoreCallbackCallerHelper is made on behalf of its Program, also when
        // it is a builtin call
        const QoreProgram* caller_code_pgm;
        runtime_take_call_caller_parse_options(caller_code_pgm);

        QoreProgram* frame_pgm = nullptr;
        if (pgm_ctx) {
            assert(pgm_ctx == ::getProgram());
            frame_pgm = qore_get_call_program_context();
            ptcch.set(xsink, pgm_ctx, true);
            if (*xsink) {
                return;
            }
            QoreParseOptions pgm_po = qore_program_private::getParseOptions(*pgm_ctx);
            if (pgm_ctx != frame_pgm || runtime_get_parse_options() != pgm_po) {
                old_rc_po = rc.getParseOptions();
                rc.setParseOptions(pgm_po);
                swap_runtime_parse_options(pgm_po, old_runtime_stmt, old_runtime_ctx_loc, old_runtime_po);
                restore_runtime_ctx = true;
            }
        }

        // the signature's return type, as CodeEvaluationHelper::prepareCallArgumentsImpl() gives it
        old_return_type = saveReturnTypeInfo(variant.getSignature()->getReturnTypeInfo());
        restore_return_type = true;
        old_rtflags = QoreRuntimeFlags::get(rc);
        QoreRuntimeFlags::set(rc, static_cast<q_rt_flags_t>(variant.getFlags()));
        restore_rtflags = true;
        stack_loc = update_get_runtime_stack_builtin_location(this, stmt, pgm, old_runtime_loc, frame_pgm);
        restore_stack = true;
    }

    ~QoreAOTBuiltinCallFrame() override {
        if (restore_stack) {
            update_runtime_stack_location(stack_loc, old_runtime_loc);
        }
        if (restore_runtime_ctx) {
            rc.setParseOptions(old_rc_po);
            update_runtime_statement_location(old_runtime_stmt, old_runtime_ctx_loc, old_runtime_po);
        }
        if (restore_rtflags) {
            QoreRuntimeFlags::set(rc, old_rtflags);
        }
        if (restore_return_type) {
            saveReturnTypeInfo(old_return_type);
        }
    }

    //! true if the call can be made: its frame is in place
    bool ready() const {
        return restore_stack;
    }

    QoreProgram* getProgram() const override {
        return pgm;
    }

    const AbstractStatement* getStatement() const override {
        return stmt;
    }

    const std::string& getCallName() const override {
        return call_name;
    }

    qore_call_t getCallType() const override {
        return CT_BUILTIN;
    }

    const QoreProgramLocation& getLocation() const override {
        return loc ? *loc : loc_builtin;
    }

    QoreAOTBuiltinCallFrame(const QoreAOTBuiltinCallFrame&) = delete;
    QoreAOTBuiltinCallFrame& operator=(const QoreAOTBuiltinCallFrame&) = delete;

private:
    //! the entry of the call's Program; destroyed after the frame is restored, as CodeEvaluationHelper's base
    ProgramThreadCountContextHelper ptcch;
    RuntimeConfig& rc;
    const std::string& call_name;
    const QoreProgramLocation* loc;
    const AbstractStatement* stmt = nullptr;
    QoreProgram* pgm = nullptr;
    const QoreProgramLocation* old_runtime_loc = nullptr;
    const QoreStackLocation* stack_loc = nullptr;
    const QoreTypeInfo* old_return_type = nullptr;
    q_rt_flags_t old_rtflags = 0;
    QoreParseOptions old_rc_po;
    QoreParseOptions old_runtime_po;
    const AbstractStatement* old_runtime_stmt = nullptr;
    const QoreProgramLocation* old_runtime_ctx_loc = nullptr;
    bool restore_stack = false;
    bool restore_runtime_ctx = false;
    bool restore_rtflags = false;
    bool restore_return_type = false;
};
}

extern "C" DLLEXPORT uint64_t qore_rt_pseudo_string_split_guarded_aot(QoreAOTContext* ctx, int32_t slot,
        uint64_t base_bits, uint64_t separator_bits, uint64_t** arg_cleanups, ExceptionSink* xsink) {
    assert(ctx && ctx->call_targets && slot >= 0 && slot < ctx->num_exprs && xsink);
    QoreStringDataHelper str(fromBits(base_bits));
    QoreStringDataHelper separator(fromBits(separator_bits));
    if (!str || !separator || qore_aot_string_call_needs_ordinary_path()) [[unlikely]] {
        // the ordinary call, which takes the argument over as the caller gave it
        return qore_rt_dot_eval_pseudo_method_direct_aot_consume_args(ctx, slot, base_bits, &separator_bits,
            arg_cleanups, 1, xsink);
    }
    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    assert(target.is_pseudo && target.method && target.variant && target.qc);
    assert(target.qc == qore_pseudo_get_class(NT_STRING));
    // the pseudo-class has no Program: the call is made in the caller's (see qore_get_method_pgm_context())
    assert(!qore_class_private::get(*target.qc)->ns);

    static const std::string call_name = "<string>::split";
    RuntimeConfig& rc = rc_get_current_ref();
    QoreAOTBuiltinCallFrame frame(rc, call_name, *target.variant, nullptr, xsink);
    if (!frame.ready()) {
        return toBits(QoreValue());
    }
    CodeContextHelper cch(xsink, CT_BUILTIN, "split", nullptr, runtime_get_class());
    return toBits(qore_eval_builtin_call(xsink, [&]() -> QoreValue {
        // the canonical variant's body (PseudoString_split_VsVbVb) with with_separator and keep_trailing_empty False
        const QoreEncoding* enc = str.getEncoding();
        if (separator.getEncoding() == enc) {
            return split_intern(separator.c_str(), separator.size(), str.c_str(), str.size(), enc, false, false,
                xsink);
        }
        QoreStringValueHelper separator_string(fromBits(separator_bits));
        TempEncodingHelper temp(**separator_string, enc, xsink);
        if (*xsink) {
            return QoreValue();
        }
        return split_intern(temp->c_str(), temp->strlen(), str.c_str(), str.size(), enc, false, false, xsink);
    }));
}

extern "C" DLLEXPORT uint64_t qore_rt_string_trim_guarded_aot(QoreAOTContext* ctx, int32_t slot,
        uint64_t value_bits, uint64_t** arg_cleanups, ExceptionSink* xsink) {
    assert(ctx && ctx->call_targets && slot >= 0 && slot < ctx->num_exprs && xsink);
    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }
    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    QoreValue value = fromBits(value_bits);
    // the Program of the call (see qore_rt_call_function_direct_impl()), which the builtin runs in
    QoreProgram* pgm = nullptr;
    RuntimeConfig& rc = rc_get_current_ref();
    if (QoreStringDataHelper(value) && target.func && target.variant && !target.variant->isUser()
            && !qore_aot_string_call_needs_ordinary_path()) [[likely]] {
        pgm = target.pgm ? target.pgm : rc.getProgram();
        if (!pgm) {
            pgm = ::getProgram();
        }
        if (pgm != ::getProgram()) {
            pgm = nullptr;
        }
    }
    if (!pgm) [[unlikely]] {
        return qore_rt_call_direct_aot_consume_args(ctx, slot, &value_bits, arg_cleanups, 1, xsink);
    }
    static const std::string call_name = "trim";
    // the caller's call Program context, as QoreFunction::evalFunctionTmpArgs() sets it
    ProgramCallContextHelper pcch(pgm);
    QoreAOTBuiltinCallFrame frame(rc, call_name, *target.variant, pgm, xsink);
    if (!frame.ready()) {
        return toBits(QoreValue());
    }
    CodeContextHelper cch(xsink, CT_BUILTIN, "trim");
    return toBits(qore_eval_builtin_call(xsink, [&]() -> QoreValue {
        // the canonical variant's body (f_trim_VsNs) with no characters: the decoded characters are trimmed, so text
        // that is not valid in its encoding raises an exception
        QoreStringNodeValueHelper str(value);
        SimpleRefHolder<QoreStringNode> rv(str->copy());
        return rv->trim(xsink, nullptr) ? QoreValue() : rv.release();
    }));
}

// the ordinary calls that the string fast paths make for an exception (QoreStringFastPathCalls.h): the call of a
// pseudo-method by name, as the AST interpreter makes it for a value (pseudo_classes_eval()), or of the Qore builtin
// function by name in the current Program, as qore_rt_builtin_function_call() makes it; out of line and cold, as they
// are made only for an exception
namespace {
//! the argument list of a call: the borrowed values (referenced), then the integers (owned by the list)
QoreListNode* qore_string_call_args(std::initializer_list<QoreValue> borrowed, std::initializer_list<int64> ints,
        ExceptionSink* xsink) {
    if (!borrowed.size() && !ints.size()) {
        return nullptr;
    }
    ReferenceHolder<QoreListNode> arg_list(new QoreListNode(autoTypeInfo), xsink);
    for (QoreValue arg : borrowed) {
        arg_list->push(arg.refSelf(), xsink);
    }
    for (int64 arg : ints) {
        // an integer outside the inline range is a new node, which the list takes over
        arg_list->push(QoreValue(arg), xsink);
    }
    return arg_list.release();
}

QORE_INTERN_NOINLINE __attribute__((cold)) QoreValue qore_string_pseudo_call(QoreValue str, const char* name,
        std::initializer_list<QoreValue> borrowed, std::initializer_list<int64> ints, ExceptionSink* xsink) {
    assert(xsink);
    ReferenceHolder<QoreListNode> arg_list(qore_string_call_args(borrowed, ints, xsink), xsink);
    return pseudo_classes_eval(str, name, *arg_list, xsink);
}

QORE_INTERN_NOINLINE __attribute__((cold)) QoreValue qore_string_function_call(const char* qualified_name,
        QoreValue str, std::initializer_list<QoreValue> borrowed, std::initializer_list<int64> ints,
        ExceptionSink* xsink) {
    assert(xsink);
    assert(!strncmp(qualified_name, "Qore::", 6));
    QoreProgram* pgm = getProgram();
    const QoreFunction* func = pgm ? reinterpret_cast<const QoreFunction*>(pgm->findFunction(qualified_name))
        : nullptr;
    if (!func) {
        // as qore_rt_builtin_function_call() for a Program without the builtin
        xsink->raiseException("RUNTIME-OVERLOAD-ERROR", "no variant matching '%s(%s)' can be found",
            qualified_name + 6, str.getTypeName());
        return QoreValue();
    }
    ReferenceHolder<QoreListNode> arg_list(new QoreListNode(autoTypeInfo), xsink);
    arg_list->push(str.refSelf(), xsink);
    for (QoreValue arg : borrowed) {
        arg_list->push(arg.refSelf(), xsink);
    }
    for (int64 arg : ints) {
        arg_list->push(QoreValue(arg), xsink);
    }
    return func->evalFunction(nullptr, *arg_list, pgm, rc_get_current_ref(), xsink);
}
}

QoreValue qore_string_call_case(QoreValue str, bool upper, bool function, ExceptionSink* xsink) {
    if (function) {
        return qore_string_function_call(upper ? "Qore::toupper" : "Qore::tolower", str, {}, {}, xsink);
    }
    return qore_string_pseudo_call(str, upper ? "upr" : "lwr", {}, {}, xsink);
}

QoreValue qore_string_call_length(QoreValue str, bool function, ExceptionSink* xsink) {
    if (function) {
        return qore_string_function_call("Qore::length", str, {}, {}, xsink);
    }
    return qore_string_pseudo_call(str, "length", {}, {}, xsink);
}

QoreValue qore_string_call_size(QoreValue str, ExceptionSink* xsink) {
    return qore_string_pseudo_call(str, "size", {}, {}, xsink);
}

QoreValue qore_string_call_substr(QoreValue str, int64 start, int64 length, bool has_length, bool function,
        ExceptionSink* xsink) {
    if (function) {
        return has_length
            ? qore_string_function_call("Qore::substr", str, {}, {start, length}, xsink)
            : qore_string_function_call("Qore::substr", str, {}, {start}, xsink);
    }
    return has_length
        ? qore_string_pseudo_call(str, "substr", {}, {start, length}, xsink)
        : qore_string_pseudo_call(str, "substr", {}, {start}, xsink);
}

QoreValue qore_string_call_search(QoreValue str, QoreValue pattern, QoreStringSearchCall call, int64 offset,
        ExceptionSink* xsink) {
    switch (call) {
        case QoreStringSearchCall::StartsWith:
            return qore_string_pseudo_call(str, "startsWith", {pattern}, {}, xsink);
        case QoreStringSearchCall::EndsWith:
            return qore_string_pseudo_call(str, "endsWith", {pattern}, {}, xsink);
        case QoreStringSearchCall::Contains:
            return qore_string_pseudo_call(str, "contains", {pattern}, {}, xsink);
        case QoreStringSearchCall::Find:
            return qore_string_pseudo_call(str, "find", {pattern}, {offset}, xsink);
        case QoreStringSearchCall::RFind:
            return qore_string_pseudo_call(str, "rfind", {pattern}, {offset}, xsink);
    }
    assert(false);
    return QoreValue();
}

QoreValue qore_string_call_index(QoreValue str, QoreValue pattern, bool reverse, int64 offset, ExceptionSink* xsink) {
    return qore_string_function_call(reverse ? "Qore::rindex" : "Qore::index", str, {pattern}, {offset}, xsink);
}

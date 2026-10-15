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

#include "qore/intern/QoreJITIncludes.h"
#include "qore/intern/BuiltinFunction.h"
#include "qore/intern/RuntimeConfig.h"
#include "qore/intern/qore_thread_intern.h"

#include <cstdlib>

namespace {
// The canonical string pseudo-class has no source Program or namespace. Its split
// signature has no receiver/type parameters or effectful defaults. Keep the part
// of CodeEvaluationHelper that is observable during this builtin, without its
// argument list, variant search, default evaluation, or Program switching state.
class QoreAOTStringSplitFrame final : public QoreStackLocation {
public:
    QoreAOTStringSplitFrame(RuntimeConfig& rc, const AbstractQoreFunctionVariant& variant)
        : rc(rc), loc(get_runtime_location()), old_flags(rc.getRuntimeFlags()),
          old_return_type(saveReturnTypeInfo(variant.getReturnTypeInfo())) {
        rc.setRuntimeFlags(static_cast<q_rt_flags_t>(variant.getFlags()));
        old_stack = update_get_runtime_stack_builtin_location(this, stmt, pgm, old_runtime_loc);
    }

    ~QoreAOTStringSplitFrame() override {
        // Unpublish before destroying any state that another thread can inspect.
        update_runtime_stack_location(old_stack, old_runtime_loc);
        rc.setRuntimeFlags(old_flags);
        saveReturnTypeInfo(old_return_type);
    }

    QoreProgram* getProgram() const override { return pgm; }
    const AbstractStatement* getStatement() const override { return stmt; }
    const std::string& getCallName() const override {
        static const std::string name = "<string>::split";
        return name;
    }
    qore_call_t getCallType() const override { return CT_BUILTIN; }
    const QoreProgramLocation& getLocation() const override { return loc ? *loc : loc_builtin; }

private:
    RuntimeConfig& rc;
    const QoreProgramLocation* loc;
    q_rt_flags_t old_flags;
    const QoreTypeInfo* old_return_type;
    const AbstractStatement* stmt = nullptr;
    QoreProgram* pgm = nullptr;
    const QoreProgramLocation* old_runtime_loc = nullptr;
    const QoreStackLocation* old_stack;
};
}

extern "C" DLLEXPORT uint64_t qore_rt_pseudo_string_split_aot_v1(QoreAOTContext* ctx,
        int32_t slot, uint64_t base_bits, uint64_t separator_bits, uint64_t** arg_cleanups,
        ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs && xsink);
    static const bool enabled = !std::getenv("QORE_DISABLE_AOT_STRING_SPLIT");
    QoreValue base = fromBits(base_bits);
    QoreValue separator = fromBits(separator_bits);
    if (!enabled || base.isEnum() || separator.isEnum()
            || base.getType() != NT_STRING || separator.getType() != NT_STRING) {
        uint64_t args[] = {separator_bits};
        return qore_rt_dot_eval_pseudo_method_direct_aot_consume_args(ctx, slot,
            base_bits, args, arg_cleanups, 1, xsink);
    }
    if (*xsink) {
        return toBits(QoreValue());
    }
#ifdef QORE_MANAGE_STACK
    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }
#endif
    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    assert(target.is_pseudo && target.method && target.variant && target.qc);
    assert(!qore_class_private::get(*target.qc)->spgm && !qore_class_private::get(*target.qc)->ns);
    RuntimeConfig& rc = rc_get_current_ref();
    QoreAOTStringSplitFrame frame(rc, *target.variant);
    CodeContextHelper context(xsink, CT_BUILTIN, "split", nullptr, runtime_get_class());
    return toBits(qore_eval_builtin_call(xsink, [&]() -> QoreValue {
        QoreStringDataHelper str(base);
        QoreStringDataHelper pattern(separator);
        if (pattern.getEncoding() == str.getEncoding()) {
            return split_intern(pattern.c_str(), pattern.size(), str.c_str(), str.size(),
                str.getEncoding(), false, false, xsink);
        }
        QoreStringValueHelper pattern_string(separator);
        TempEncodingHelper encoded(*pattern_string, str.getEncoding(), xsink);
        if (*xsink) {
            return QoreValue();
        }
        return split_intern(encoded->c_str(), encoded->strlen(), str.c_str(), str.size(),
            str.getEncoding(), false, false, xsink);
    }));
}

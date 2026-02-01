/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreJIT.cpp

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

#include "qore/intern/QoreJIT.h"

#ifdef QORE_JIT_ENABLED
#include <llvm/Support/Error.h>
#include <llvm/Support/TargetSelect.h>
#include "qore/intern/QoreIRToLLVM.h"
#endif

#include "qore/intern/QoreIRInterpreter.h"

QoreJIT& QoreJIT::instance() {
    static QoreJIT jit;
    return jit;
}

bool QoreJIT::isEnabled() const {
#ifdef QORE_JIT_ENABLED
    return true;
#else
    return false;
#endif
}

bool QoreJIT::canJit(int64 parse_options, std::string& reason) const {
    if ((parse_options & PO_MODERN) != PO_MODERN) {
        reason = "requires %modern (PO_MODERN)";
        return false;
    }
    return true;
}

bool QoreJIT::initialize(std::string& error) {
    if (initialized) {
        return true;
    }
#ifdef QORE_JIT_ENABLED
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    auto jit_or_err = llvm::orc::LLJITBuilder().create();
    if (!jit_or_err) {
        error = llvm::toString(jit_or_err.takeError());
        return false;
    }
    jit = std::move(*jit_or_err);
    initialized = true;
    return true;
#else
    error = "JIT support is not enabled in this build";
    return false;
#endif
}

bool QoreJIT::compileFunction(const QoreIRFunction& func, std::string& error) {
#ifdef QORE_JIT_ENABLED
    if (!initialized && !initialize(error)) {
        return false;
    }
    llvm::LLVMContext ctx;
    llvm::Module module("qore_jit_module", ctx);
    QoreIRToLLVM lowering(ctx);
    return lowering.lowerFunction(func, module, error);
#else
    error = "JIT support is not enabled in this build";
    return false;
#endif
}

bool QoreJIT::executeWithFallback(const QoreIRFunction& func, QoreValue& return_value, ExceptionSink* xsink,
        std::string& error, const std::unordered_set<const LocalVar*>* pre_instantiated) {
#ifdef QORE_JIT_ENABLED
    if (!compileFunction(func, error)) {
        if (deopt_policy == DeoptPolicy::DisableJit) {
            return false;
        }
        return QoreIRInterpreter::execute(func, return_value, xsink, nullptr, nullptr, nullptr,
            pre_instantiated);
    }
    // JIT execution will replace this once lowering is implemented.
    return QoreIRInterpreter::execute(func, return_value, xsink, nullptr, nullptr, nullptr,
        pre_instantiated);
#else
    error = "JIT support is not enabled in this build";
    return QoreIRInterpreter::execute(func, return_value, xsink, nullptr);
#endif
}

uint64_t QoreJIT::recordExecution() {
    return ++exec_count;
}

bool QoreJIT::shouldJit(uint64_t count) const {
    return count >= hot_threshold;
}

void QoreJIT::recordTypeProfile(const QoreValue& value) {
    ++type_profile[value.getType()];
}

void QoreJIT::setDeoptPolicy(DeoptPolicy policy) {
    deopt_policy = policy;
}

void QoreJIT::setOsrEnabled(bool enable) {
    osr_enabled = enable;
}

void QoreJIT::shutdown() {
#ifdef QORE_JIT_ENABLED
    jit.reset();
#endif
    initialized = false;
}

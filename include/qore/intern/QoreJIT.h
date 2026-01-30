/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreJIT.h

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

#ifndef _QORE_QOREJIT_H
#define _QORE_QOREJIT_H

#include <memory>
#include <string>
#include <atomic>
#include <unordered_map>

#include <qore/QoreValue.h>
#include <qore/Restrictions.h>

class ExceptionSink;

#ifdef QORE_JIT_ENABLED
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#endif

class QoreJIT {
public:
    enum class DeoptPolicy {
        FallbackToInterpreter,
        DisableJit,
    };
    static QoreJIT& instance();

    bool isEnabled() const;
    bool initialize(std::string& error);
    bool canJit(int64 parse_options, std::string& reason) const;
    bool compileFunction(const class QoreIRFunction& func, std::string& error);
    bool executeWithFallback(const class QoreIRFunction& func, QoreValue& return_value, ExceptionSink* xsink,
            std::string& error);
    void shutdown();
    uint64 recordExecution();
    bool shouldJit(uint64 count) const;
    void recordTypeProfile(const QoreValue& value);
    void setDeoptPolicy(DeoptPolicy policy);

private:
    QoreJIT() = default;
    QoreJIT(const QoreJIT&) = delete;
    QoreJIT& operator=(const QoreJIT&) = delete;

#ifdef QORE_JIT_ENABLED
    std::unique_ptr<llvm::orc::LLJIT> jit;
#endif
    bool initialized = false;
    std::atomic<uint64> exec_count{0};
    uint64 hot_threshold = 1000;
    std::unordered_map<qore_type_t, uint64> type_profile;
    DeoptPolicy deopt_policy = DeoptPolicy::FallbackToInterpreter;
};

#endif

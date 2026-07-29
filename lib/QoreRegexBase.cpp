/*
    QoreRegexBase.cpp

    regular expression substitution node definition

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

#include <qore/Qore.h>
#include "qore/intern/QoreRegexBase.h"

namespace {
//! The calling thread's PCRE2 match context and its JIT stack.
/** A JIT stack must not be used by two threads at the same time, so each thread owns one; the match
    context exists to carry it.

    This is deliberately a trivially-destructible aggregate: it therefore has no thread-exit
    destructor of its own and its storage stays valid for the entire life of the thread.  The
    pointers are released by qore_pcre2_thread_cleanup below, which does have a destructor; keeping
    the two separate means a regular expression evaluated after that cleanup has run (for example
    from an object destructor executed during thread teardown, which on glibc happens after C++
    thread_local destructors) safely falls back to PCRE2's defaults instead of using a freed
    context.
 */
struct qore_pcre2_thread_data {
    pcre2_match_context* mc = nullptr;
    pcre2_jit_stack* js = nullptr;
    //! set when the context could not be created or has already been released; no (re)allocation
    bool disabled = false;
};

thread_local qore_pcre2_thread_data q_pcre2_td;

//! Releases the calling thread's match context and JIT stack when the thread terminates
class qore_pcre2_thread_cleanup {
public:
    ~qore_pcre2_thread_cleanup() {
        q_pcre2_td.disabled = true;
        // the context is released first; it refers to the JIT stack
        if (q_pcre2_td.mc) {
            pcre2_match_context_free(q_pcre2_td.mc);
            q_pcre2_td.mc = nullptr;
        }
        if (q_pcre2_td.js) {
            pcre2_jit_stack_free(q_pcre2_td.js);
            q_pcre2_td.js = nullptr;
        }
    }
};

//! Returns the calling thread's match context, creating it on first use
/** Returns nullptr if the context could not be created or has already been released at thread exit;
    pcre2_match() then applies its own defaults, which is correct, just without our JIT stack.
 */
pcre2_match_context* qore_pcre2_thread_match_context() {
    if (q_pcre2_td.mc || q_pcre2_td.disabled) {
        return q_pcre2_td.mc;
    }

    // registers the thread-exit cleanup on first use
    static thread_local qore_pcre2_thread_cleanup cleanup;
    (void)cleanup;

    pcre2_match_context* mc = pcre2_match_context_create(nullptr);
    if (!mc) {
        // do not retry on every match
        q_pcre2_td.disabled = true;
        return nullptr;
    }
    // the JIT stack is optional; without one the JIT uses a 32 KB buffer on the machine stack
    q_pcre2_td.js = pcre2_jit_stack_create(qore_pcre2_jit_stack_start_size, qore_pcre2_jit_stack_max_size,
        nullptr);
    if (q_pcre2_td.js) {
        pcre2_jit_stack_assign(mc, nullptr, q_pcre2_td.js);
    }
    q_pcre2_td.mc = mc;
    return mc;
}
}

void QoreRegexBase::jitCompile() {
    assert(p);
    // ignore errors; PCRE2 uses the interpreter when no JIT code is attached
    pcre2_jit_compile(p, PCRE2_JIT_COMPLETE);
}

int qore_pcre2_match(const pcre2_code* code, PCRE2_SPTR8 subject, PCRE2_SIZE length,
        PCRE2_SIZE startoffset, uint32_t options, pcre2_match_data* md) {
    int rc = pcre2_match(code, subject, length, startoffset, options, md, qore_pcre2_thread_match_context());
    if (rc == PCRE2_ERROR_JIT_STACKLIMIT) {
        // the pattern needs more backtracking stack than qore_pcre2_jit_stack_max_size; the
        // interpreter uses heap frames bounded by PCRE2's heap limit instead, so retry with it
        rc = pcre2_match(code, subject, length, startoffset, options | PCRE2_NO_JIT, md,
            qore_pcre2_thread_match_context());
    }
    return rc;
}

int qore_pcre2_check_match_error(int rc, ExceptionSink* xsink) {
    assert(rc < 1);
    if (qore_pcre2_expected_match_error(rc)) {
        return 0;
    }

    if (xsink) {
        PCRE2_UCHAR buffer[qore_pcre2_errorbuf_size];
        const char* msg = pcre2_get_error_message(rc, buffer, sizeof(buffer)) < 0
            ? "unknown error"
            : reinterpret_cast<const char*>(buffer);
        switch (rc) {
            case PCRE2_ERROR_MATCHLIMIT:
            case PCRE2_ERROR_DEPTHLIMIT:
            case PCRE2_ERROR_HEAPLIMIT:
                xsink->raiseException("REGEX-ERROR", "the regular expression engine gave up before it could "
                    "determine a result; the pattern requires too much backtracking for this subject string "
                    "(PCRE2 error %d: %s)", rc, msg);
                break;
            default:
                xsink->raiseException("REGEX-ERROR", "regular expression matching failed with PCRE2 error %d: %s",
                    rc, msg);
                break;
        }
    }
    return -1;
}

void QoreRegexBase::setCaseInsensitive() {
    options |= PCRE2_CASELESS;
}

void QoreRegexBase::setDotAll() {
    options |= PCRE2_DOTALL;
}

void QoreRegexBase::setExtended() {
    options |= PCRE2_EXTENDED;
}

void QoreRegexBase::setMultiline() {
    options |= PCRE2_MULTILINE;
}

void QoreRegexBase::setUnicode() {
    options |= PCRE2_UCP;
}

void QoreRegexBase::setUngreedy() {
    options |= PCRE2_UNGREEDY;
}


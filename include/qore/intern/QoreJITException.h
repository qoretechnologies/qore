/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreJITException.h

    C++ exception type for JIT/AOT-compiled Qore code

    When a qore_rt_* function sets xsink, qore_rt_check_throw() throws
    this exception. LLVM's invoke/landingpad mechanism catches it for
    stack unwinding, matching C++ exception handling semantics.

    This replaces the manual per-instruction xsink flag checking that
    produced pathological LLVM IR (73K+ conditional branches in large
    functions).

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

#ifndef _QORE_INTERN_QOREJITEXCEPTION_H
#define _QORE_INTERN_QOREJITEXCEPTION_H

#include <exception>

//! Clears the current thread's call site (see QoreRcCallSiteHelper)
DLLLOCAL void clear_runtime_call_site();

//! C++ exception thrown by JIT/AOT-compiled code when xsink has an exception
/** Used with LLVM's invoke/landingpad mechanism for proper stack unwinding.
    The exception carries a pointer to the ExceptionSink that was set.
    Catching code should NOT clear xsink — it's owned by the caller.
*/
class QoreJITException : public std::exception {
public:
    //! Creates the exception; the call site of a call that raised it is cleared
    /** Native code clears the call site after each call on the normal path; when a call raises an exception
        before the frame of the called code has taken the call site, the exception clears it, so that it is never
        taken by another frame.
    */
    DLLLOCAL QoreJITException() noexcept {
        clear_runtime_call_site();
    }
    DLLLOCAL const char* what() const noexcept override {
        return "QoreJITException";
    }
};

#endif // _QORE_INTERN_QOREJITEXCEPTION_H

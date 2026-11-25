/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAllocFailure.h

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

#ifndef _QORE_INTERN_QOREALLOCFAILURE_H
#define _QORE_INTERN_QOREALLOCFAILURE_H

#include "qore/common.h"

#include <cstdlib>
#include <new>

class ExceptionSink;

/** @file QoreAllocFailure.h

    How the runtime handles memory that cannot be allocated (qore#7466): the allocation of a string's or a list's
    storage fails with std::bad_alloc and leaves the value unchanged, and the std::bad_alloc becomes a Qore exception
    (MEMORY-ALLOCATION-ERROR) at the call of builtin code or at the statement that needed the memory
*/

#ifdef DEBUG
//! Makes the \a n th allocation of a string's or a list's storage of at least \a min_size bytes that follows in the
//! current thread fail with std::bad_alloc (debug builds only; qore#7466); 0 disarms it
DLLLOCAL void qore_dbg_fail_allocation(int64 n, size_t min_size);

//! true if the debug hook armed with qore_dbg_fail_allocation() makes this allocation of \a size bytes fail
DLLLOCAL bool qore_dbg_allocation_fails(size_t size);
#endif

//! Throws std::bad_alloc; out of line, so that the allocation functions below add only a test to the code that calls
//! them
[[noreturn]] DLLLOCAL void qore_throw_bad_alloc();

//! realloc(), which the debug hook (qore_dbg_fail_allocation()) can make fail: for the growth paths that handle the
//! failure themselves, so that the code that does not fail stays as it was
DLLLOCAL inline void* qore_realloc_or_null(void* ptr, size_t size) {
#ifdef DEBUG
    if (qore_dbg_allocation_fails(size)) {
        return nullptr;
    }
#endif
    return realloc(ptr, size);
}

//! realloc() for the storage of a value: memory that cannot be allocated fails with std::bad_alloc, and \a ptr is then
//! unchanged and still owned by the caller; \a size must not be 0
DLLLOCAL inline void* qore_realloc_or_throw(void* ptr, size_t size) {
#ifdef DEBUG
    if (qore_dbg_allocation_fails(size)) {
        throw std::bad_alloc();
    }
#endif
    void* p = realloc(ptr, size);
    if (__builtin_expect(!p, 0)) {
        qore_throw_bad_alloc();
    }
    return p;
}

//! malloc() for the storage of a value: memory that cannot be allocated fails with std::bad_alloc; \a size must not be
//! 0
DLLLOCAL inline void* qore_malloc_or_throw(size_t size) {
#ifdef DEBUG
    if (qore_dbg_allocation_fails(size)) {
        throw std::bad_alloc();
    }
#endif
    void* p = malloc(size);
    if (__builtin_expect(!p, 0)) {
        qore_throw_bad_alloc();
    }
    return p;
}

//! Raises \c MEMORY-ALLOCATION-ERROR for an allocation that failed with std::bad_alloc (qore#7466)
/** The operation that needed the memory fails with a Qore exception, which the code can catch, instead of the process
    ending with std::terminate().  If not even the exception can be allocated, the process ends with a message on
    stderr.

    @param xsink the sink to raise the exception in
    @param context what needed the memory, for the description of the exception
*/
DLLLOCAL void qore_raise_allocation_failure(ExceptionSink* xsink, const char* context) noexcept;

//! Raises \c MEMORY-ALLOCATION-ERROR for a std::bad_alloc that left compiled code, or ends the process (qore#7466)
/** Called by the handler of a std::bad_alloc at a boundary of the C++ code with the compiled code that it calls (the
    IR interpreter, JIT or AOT code), while the exception is handled.  The runtime helpers that allocate the storage of
    a value raise the exception themselves (QORE_RT_CATCH_ALLOCATION_FAILURE() in JITRuntime.cpp), so that the compiled
    code fails as on any exception; a std::bad_alloc that reaches a boundary comes from an allocation that no helper
    handles.  The frames of native (JIT, AOT) code that it left released nothing and kept what they had pushed: when
    the function whose allocation failed was the only frame of native code that the exception left, the call fails
    with the exception, which the caller can catch; when frames of native code that it called directly were left too,
    which have pushed their frames on the thread's call stack in their own storage (qore_native_frames_left()), the
    thread cannot go on, and the process ends with std::terminate() (the message of the terminate handler).  Whether
    native frames were left is only known here, from the addresses: nothing is recorded on the path that does not
    fail.

    @param xsink the sink to raise the exception in
*/
DLLLOCAL void qore_raise_compiled_code_allocation_failure(ExceptionSink* xsink) noexcept
    __attribute__((noinline, cold));

//! true if the thread's current call stack location is in the part of its stack below \a boundary (qore#7466)
/** @param boundary an address in the stack frame of a function called by the boundary that handles a std::bad_alloc:
    the frames of native code that the exception left were below it
*/
DLLLOCAL bool qore_native_frames_left(const void* boundary);

//! Installs the terminate handler that reports a std::bad_alloc that no code could handle before the process ends,
//! and holds back the memory for the exception of an allocation that fails (qore#7466); called by qore_init()
DLLLOCAL void qore_install_terminate_handler();

//! Takes the memory held back for the exception of an allocation that failed again if it was released, when there is
//! memory for it (qore#7466); called when an exception is deleted, after the code that failed has released its memory
DLLLOCAL void qore_check_allocation_failure_reserve();

#endif

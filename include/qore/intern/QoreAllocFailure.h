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

//! realloc() for the storage of a value: memory that cannot be allocated fails with std::bad_alloc, and \a ptr is then
//! unchanged and still owned by the caller
DLLLOCAL inline void* qore_realloc_or_throw(void* ptr, size_t size) {
#ifdef DEBUG
    if (qore_dbg_allocation_fails(size)) {
        throw std::bad_alloc();
    }
#endif
    void* p = realloc(ptr, size);
    if (!p && size) {
        throw std::bad_alloc();
    }
    return p;
}

//! malloc() for the storage of a value: memory that cannot be allocated fails with std::bad_alloc
DLLLOCAL inline void* qore_malloc_or_throw(size_t size) {
#ifdef DEBUG
    if (qore_dbg_allocation_fails(size)) {
        throw std::bad_alloc();
    }
#endif
    void* p = malloc(size);
    if (!p && size) {
        throw std::bad_alloc();
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

//! Installs the terminate handler that reports a std::bad_alloc that no code could handle before the process ends,
//! and holds back the memory for the exception of an allocation that fails (qore#7466); called by qore_init()
DLLLOCAL void qore_install_terminate_handler();

//! Takes the memory held back for the exception of an allocation that failed again if it was released, when there is
//! memory for it (qore#7466); called when an exception is deleted, after the code that failed has released its memory
DLLLOCAL void qore_check_allocation_failure_reserve();

#endif

/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAOTMethodDispatchReclaim.h

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

#ifndef _QORE_INTERN_QOREAOTMETHODDISPATCHRECLAIM_H
#define _QORE_INTERN_QOREAOTMETHODDISPATCHRECLAIM_H

#include "qore/QoreParseOptions.h"
#include "qore/intern/QoreEpochReclaimer.h"

class qore_class_private;
class QoreProgram;

//! true if every running thread of the process can be made to issue a full memory barrier (lib/thread.cpp)
/** Set once in init_qore_threads(), before any other thread exists, and never changed
*/
DLLLOCAL extern bool stack_walk_heavy_barrier;

//! issues a full memory barrier on every running thread of the process; only if stack_walk_heavy_barrier
DLLLOCAL void qore_process_heavy_barrier();

//! the reclamation domain of the replaced AOT method dispatch cache entries (qore#7459)
/** Lookups are frequent and replacements rare, so with a process-wide barrier available, a lookup's read section
    has only a compiler barrier and a replacement issues the process-wide barrier (as the call stack's pops and walks
    do)
*/
struct DLLLOCAL QoreAOTMethodDispatchReclaimTag {
    static bool lightReaders() noexcept {
        return stack_walk_heavy_barrier;
    }

    static void heavyBarrier() noexcept {
        qore_process_heavy_barrier();
    }
};

typedef QoreEpochReclaimer<QoreAOTMethodDispatchReclaimTag> QoreAOTMethodDispatchReclaimer;

//! what a method call that AOT code dispatches by name needs from the calling thread's data
struct DLLLOCAL QoreAOTDispatchThreadContext {
    //! the class context of the running code (runtime_get_class())
    const qore_class_private* class_ctx = nullptr;
    //! the running Program (getProgram())
    QoreProgram* pgm = nullptr;
    //! the runtime parse options (runtime_get_parse_options())
    QoreParseOptions parse_options;
    //! the thread's record of the reclamation domain, kept with the thread's data; nullptr until the thread's first
    //! cacheable dispatch acquires it (see qore_acquire_aot_dispatch_thread_reader())
    QoreAOTMethodDispatchReclaimer::Record* reader = nullptr;
};

//! returns the dispatch context of the calling Qore thread with one lookup of its thread data (lib/thread.cpp)
DLLLOCAL void qore_get_aot_dispatch_thread_context(QoreAOTDispatchThreadContext& ctx);

//! returns the calling Qore thread's record of the reclamation domain, acquiring it first (lib/thread.cpp)
/** Kept with the thread's data and released when the data is deleted.  Returns nullptr without thread data, once
    the record has been released, or if none could be allocated (which is not retried): the thread's lookups then
    miss.
*/
DLLLOCAL QoreAOTMethodDispatchReclaimer::Record* qore_acquire_aot_dispatch_thread_reader();

#endif

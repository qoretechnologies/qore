/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    AllocTracking.h

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

#ifndef _QORE_INTERN_ALLOCTRACKING_H
#define _QORE_INTERN_ALLOCTRACKING_H

#ifdef QORE_HAVE_ALLOC_TRACKING

#include <cstdint>

class QoreHashNode;
class ExceptionSink;

//! Start tracking memory allocations
/** Clears any previous tracking data and begins recording all malloc/free calls
    with backtrace information. Only one tracking session can be active at a time.
*/
void qore_alloc_tracking_start();

//! Stop tracking and return results
/** Stops tracking and returns a hash keyed by backtrace hash (hex string).
    Each value is a hash containing:
    - \c alloc_count: number of allocations from this call site
    - \c alloc_bytes: total bytes allocated
    - \c free_count: number of frees matched to this site
    - \c free_bytes: total bytes freed
    - \c leak_bytes: alloc_bytes - free_bytes
    - \c frames: list of backtrace frame strings

    @param xsink exception sink for error reporting
    @return hash with allocation statistics, or nullptr on error
*/
QoreHashNode* qore_alloc_tracking_stop(ExceptionSink* xsink);

//! Returns true if allocation tracking is currently active
bool qore_alloc_tracking_active();

//! Track a QoreObject creation (call from QoreObject constructor when tracking is active)
void qore_alloc_tracking_object_created(const char* class_name);

//! Track a QoreObject destruction (call from QoreObject destructor when tracking is active)
void qore_alloc_tracking_object_destroyed(const char* class_name);

#endif // QORE_HAVE_ALLOC_TRACKING
#endif // _QORE_INTERN_ALLOCTRACKING_H

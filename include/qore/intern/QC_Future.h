/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_Future.h

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

#ifndef _QORE_CLASS_FUTURE_H
#define _QORE_CLASS_FUTURE_H

#include <qore/QoreFuture.h>
#include "qore/intern/QC_FutureImpl.h"

DLLEXPORT extern qore_classid_t CID_FUTURE;
DLLEXPORT extern QoreClass* QC_FUTURE;

DLLLOCAL QoreClass* initFutureClass(QoreNamespace& ns);

//! Block on any Qore @c Future object from C++, with timeout.
/** Used by sync code paths that need to wait synchronously on a Future
    returned from an async submission (e.g. the sync @c HTTPClient
    facade delegating to @c HttpClientIo via the composition pattern).

    Fast path: if @a future_obj is a @c FutureImpl (the default Promise
    backing), the C++ @ref QoreFuture::get() is called directly — no
    interpreter overhead.  Slow path: for custom Future subclasses
    (e.g. @c HttpCancellableFuture), the Qore-level
    @c Future::get(timeout) method is dispatched via @c evalMethod.

    @param future_obj the Qore Future object (must not be null)
    @param timeout_ms wait budget in milliseconds; 0 or negative means
        "wait indefinitely"
    @param xsink exception sink

    @return the resolved Future value on success; @c QoreValue() on
        timeout (raises @c FUTURE-TIMEOUT), cancellation (raises
        @c FUTURE-CANCELLED), or any exception propagated from the
        Future's @c setError() or @c setException() side

    @note The returned value is owned by the caller and must be
        dereferenced when no longer needed.

    @since %Qore 2.3
*/
DLLLOCAL QoreValue q_future_get_blocking(QoreObject* future_obj,
        int64 timeout_ms, ExceptionSink* xsink);

#endif // _QORE_CLASS_FUTURE_H

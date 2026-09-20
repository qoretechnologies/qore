/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    HttpClientEventSink.cpp

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
#include <qore/AsyncCompletionAction.h>

#include "qore/intern/HttpClientEventSink.h"

// the event sink of a request rides on the action that completes it; the definitions live here because
// AsyncCompletionAction.h is a public header and the sink is internal to the library

void AbstractAsyncAction::setEventSink(HttpClientEventSink* sink) {
    // the sink is set when the request is submitted, before the action can run
    assert(!event_sink || event_sink == sink);
    if (sink == event_sink) {
        return;
    }
    if (sink) {
        sink->ref();
    }
    HttpClientEventSink* old = event_sink;
    event_sink = sink;
    if (old) {
        // defensive: the sink is set once per action, so this path is not reached
        ExceptionSink xsink;
        old->deref(&xsink);
        xsink.clear();
    }
}

void AbstractAsyncAction::releaseEventSink(ExceptionSink* xsink) {
    if (event_sink) {
        event_sink->deref(xsink);
        event_sink = nullptr;
    }
}

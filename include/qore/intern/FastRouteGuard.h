/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    FastRouteGuard.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#ifndef _QORE_INTERN_FASTROUTEGUARD_H
#define _QORE_INTERN_FASTROUTEGUARD_H

#include <qore/common.h>

#include <cassert>

class ResolvedCallReferenceNode;
class ExceptionSink;

// Debug-build runtime guard for HTTP fast routes: while an async I/O thread answers a request from a fast route
// (see design/http-server-fast-routes.md), nothing may run Qore code or block.  The scope is entered by
// serveFastRoute(); the interpreter entry points and the blocking waits assert that no scope is active on the
// calling thread.  Release builds compile all of it away.

#ifndef NDEBUG
namespace qore_fast_route_guard {
    // depth of the fast route scopes on this thread
    DLLLOCAL extern thread_local int depth;
    // when set, a violation on this thread is counted here instead of aborting (for the test of the guard)
    DLLLOCAL extern thread_local int* probe;
    // reports a violation: counts it if a probe is set, otherwise prints the place and aborts
    DLLLOCAL void violation(const char* where);
}

class FastRouteScope {
public:
    DLLLOCAL FastRouteScope() noexcept {
        ++qore_fast_route_guard::depth;
    }
    DLLLOCAL ~FastRouteScope() noexcept {
        --qore_fast_route_guard::depth;
    }
    FastRouteScope(const FastRouteScope&) = delete;
    FastRouteScope& operator=(const FastRouteScope&) = delete;

    DLLLOCAL static bool active() noexcept {
        return qore_fast_route_guard::depth > 0;
    }
};

// runs an action in a fast route scope with the violations counted; returns the count or -1 in a build without the
// guard (for the test of the guard)
DLLLOCAL int qore_fast_route_guard_probe(const char* kind, const ResolvedCallReferenceNode* cb, ExceptionSink* xsink);

#define QORE_ASSERT_NOT_IN_FAST_ROUTE() \
    do { \
        if (FastRouteScope::active()) { \
            qore_fast_route_guard::violation(__func__); \
        } \
    } while (0)
#else
#define QORE_ASSERT_NOT_IN_FAST_ROUTE() ((void)0)
DLLLOCAL int qore_fast_route_guard_probe(const char* kind, const ResolvedCallReferenceNode* cb, ExceptionSink* xsink);
#endif

#endif

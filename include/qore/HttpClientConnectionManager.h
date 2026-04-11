/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    HttpClientConnectionManager.h

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

#ifndef _QORE_HTTPCLIENTCONNECTIONMANAGER_H

#define _QORE_HTTPCLIENTCONNECTIONMANAGER_H

#include <qore/AbstractPrivateData.h>
#include <qore/HttpClientConnection.h>

//! Abstract base for C++ HTTP client connection managers.
/** This is the prep-phase declaration of the connection manager surface
    needed by @ref HttpClientConnectionBase::onClosedHook to dispatch
    eviction calls.  Phase P3 of the porting plan
    (see @c design/http-client-manager-cpp-port.md) adds the concrete
    @c HttpClientConnectionManagerBase implementation with pool, proxy
    parsing, and acquireConnection/releaseConnection.

    @par Lock ordering
    Connection close-hook callbacks acquire locks in this order:
    @code
    Http1ClientConnection::onclose_lock        (per connection)
        ↓
    HttpClientConnectionManagerBase::pool_lock (per manager — RWLock)
        ↓
    AsyncIoControllerPriv::m                   (controller)
    @endcode
    No path may take these locks in any other order.

    @par Lifetime contract
    Subclasses MUST call @c setManager(nullptr) on every connection they
    have ever owned BEFORE freeing manager state.  Otherwise the I/O
    thread could fire @ref HttpClientConnectionBase::onClosedHook on a
    half-destroyed manager (use-after-free).

    @since %Qore 2.3
*/
class HttpClientConnectionManagerBase : public AbstractPrivateData {
public:
    DLLEXPORT virtual ~HttpClientConnectionManagerBase() = default;

    //! Called by a connection's @ref AbstractHttpPollConnectionPriv::onClosedHook
    //! when its state first transitions to CLOSED.
    /** Implementations evict the connection from their pool and (if
        applicable) cancel the associated controller submission.

        Called from any thread (typically the async I/O thread when an
        idle timeout fires or the peer closes the connection).  Must be
        non-blocking and bounded — see lock ordering above.

        @param conn the connection that has been closed; the manager
            holds a borrowed pointer (no ref).  Implementations should
            NOT @c deref the connection — that is the caller's
            responsibility.

        @since %Qore 2.3
    */
    DLLEXPORT virtual void onConnectionClosed(HttpClientConnectionBase* conn) = 0;
};

#endif // _QORE_HTTPCLIENTCONNECTIONMANAGER_H

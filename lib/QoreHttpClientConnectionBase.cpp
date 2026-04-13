/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHttpClientConnectionBase.cpp

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
#include <qore/HttpClientConnection.h>
#include <qore/HttpClientConnectionManager.h>

bool HttpClientConnectionBase::tryReserveStream() {
    AutoLocker al(reserve_lock);
    int max = getMaxConcurrentStreams();
    if (max && (getActiveStreamCount() + pending_stream_count) >= max) {
        return false;
    }
    ++pending_stream_count;
    return true;
}

void HttpClientConnectionBase::releaseStreamReservation() {
    AutoLocker al(reserve_lock);
    if (pending_stream_count > 0) {
        --pending_stream_count;
    }
}

int HttpClientConnectionBase::getPendingStreamCount() const {
    AutoLocker al(reserve_lock);
    return pending_stream_count;
}

void HttpClientConnectionBase::setManager(HttpClientConnectionManagerBase* mgr) {
    AutoLocker al(onclose_lock);
    manager_ = mgr;
}

void HttpClientConnectionBase::onClosedHook() {
    // Read the back-pointer under our local lock, then release the lock
    // BEFORE invoking the manager method.  This breaks any potential
    // ordering issues between onclose_lock and manager.pool_lock for
    // app-thread paths that take pool_lock first.
    //
    // Lifetime safety: setManager(nullptr) is contractually required to
    // run before the manager destroys itself, so a non-null manager_
    // observed here is guaranteed alive for the duration of the call.
    HttpClientConnectionManagerBase* mgr;
    {
        AutoLocker al(onclose_lock);
        mgr = manager_;
    }
    if (mgr) {
        mgr->onConnectionClosed(this);
    }
}

QoreHashNode* HttpClientConnectionBase::submitRequest(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        ExceptionSink* xsink) {
    // Default: not implemented.  C++ subclasses (Http1ClientConnection etc.)
    // override with protocol-specific implementations.  Qore subclasses
    // override the QPP method at the Qore level.
    xsink->raiseException("HTTPCLIENT-NOT-IMPLEMENTED",
        "submitRequest not implemented on this connection class");
    return nullptr;
}

void HttpClientConnectionBase::closeConnection(ExceptionSink* xsink) {
    // Default: just transition the state machine to CLOSED.
    // C++ subclasses override to also abort the poll op and cancel the
    // controller submission.  Qore subclasses override at the Qore level.
    setClosed();
}

bool HttpClientConnectionBase::waitForReadyOrError(int64_t timeout_ms, ExceptionSink* xsink) {
    // Delegate to the AbstractHttpPollConnectionPriv condition-variable wait.
    bool ready = AbstractHttpPollConnectionPriv::waitForReady(timeout_ms);

    if (ready) {
        return true;
    }

    // Either we timed out or the state transitioned to CLOSED / DRAINING
    // without ever reaching READY.  Distinguish CLOSED (raise error) from
    // timeout (return false quietly).
    if (isClosed()) {
        ReferenceHolder<QoreHashNode> err(getReferencedErrorInfo(), xsink);
        const char* err_str = "HTTPCLIENT-CONNECT-ERROR";
        const char* desc_str = "connection closed before READY";
        if (err) {
            QoreValue err_v = err->getKeyValue("err");
            if (err_v.getType() == NT_STRING) {
                err_str = err_v.get<const QoreStringNode>()->c_str();
            }
            QoreValue desc_v = err->getKeyValue("desc");
            if (desc_v.getType() == NT_STRING) {
                desc_str = desc_v.get<const QoreStringNode>()->c_str();
            }
        }
        xsink->raiseException(err_str, "%s", desc_str);
        return false;
    }

    // Timeout: no exception, caller decides what to do.
    return false;
}

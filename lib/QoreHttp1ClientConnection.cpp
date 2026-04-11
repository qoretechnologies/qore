/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHttp1ClientConnection.cpp

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
#include <qore/QoreSocketObject.h>
#include <qore/QoreFuture.h>
#include <qore/AsyncCompletionAction.h>
#include <qore/HttpClientConnectionManager.h>
#include "qore/intern/QoreHttp1ClientConnection.h"
#include "qore/intern/QC_Http1ClientPollOperationBase.h"
#include "qore/intern/QC_SocketPollOperation.h"
#include "qore/intern/QC_Socket.h"
#include "qore/intern/QC_FutureImpl.h"
#include "qore/intern/AsyncIoControllerPriv.h"

#include <cstdio>

// The QPP-generated .cpp for Http1ClientPollOperationBase is #included later
// in the single-compilation-unit umbrella, so these globals are not yet
// visible to the compiler when this file is processed.  Forward-declare them
// as extern here — the linker resolves them to the QPP-defined versions.
extern QoreClass* QC_HTTP1CLIENTPOLLOPERATIONBASE;
extern qore_classid_t CID_HTTP1CLIENTPOLLOPERATIONBASE;

Http1ClientConnection::Http1ClientConnection(const char* target_host, int target_port,
        bool ssl_required, ExceptionSink* xsink)
    : HttpClientConnectionBase(target_host, target_port, ssl_required) {
    if (buildAndSubmit(xsink)) {
        // buildAndSubmit raised an exception and left the partial state
        // clean (all refs released).  Caller must still deref this object.
        return;
    }
}

Http1ClientConnection::~Http1ClientConnection() {
    ExceptionSink xsink;
    closeConnection(&xsink);
    if (poll_op_obj) {
        poll_op_obj->deref(&xsink);
        poll_op_obj = nullptr;
    }
    if (sock_obj) {
        sock_obj->deref(&xsink);
        sock_obj = nullptr;
    }
    // poll_op_priv / sock_priv are owned by poll_op_obj / sock_obj — clearing
    // is unnecessary but makes use-after-free bugs easier to catch.
    poll_op_priv = nullptr;
    sock_priv = nullptr;
    xsink.clear();
}

int Http1ClientConnection::buildAndSubmit(ExceptionSink* xsink) {
    QoreProgram* pgm = getProgram();

    // Hold every fallible allocation in a RAII holder; only commit ownership
    // to member variables on the success path at the end of the function.
    // This keeps the destructor safe — if any step here fails, the holders
    // unwind and member pointers stay nullptr, so closeConnection() / dtor
    // never touches a freed object.

    // 1. Create the socket QoreSocketObject + QoreObject wrapper.
    ReferenceHolder<QoreSocketObject> sock_priv_holder(new QoreSocketObject, xsink);
    QoreSocketObject* sock_priv_raw = *sock_priv_holder;
    ReferenceHolder<QoreObject> sock_obj_holder(
        new QoreObject(QC_SOCKET, pgm, sock_priv_holder.release()), xsink);

    // 2. Build the TCP connect target string "host:port".
    char target_str[256];
    int n = snprintf(target_str, sizeof(target_str), "%s:%d", target_host.c_str(), target_port);
    if (n <= 0 || (size_t)n >= sizeof(target_str)) {
        xsink->raiseException("HTTPCLIENT-CONNECT-ERROR",
            "target host:port string too long: host='%s' port=%d",
            target_host.c_str(), target_port);
        return -1;
    }

    // 3. Create the SocketConnectPollOperation.  The ctor takes an already-ref'd
    //    QoreSocketObject pointer and adopts it.
    sock_priv_raw->ref();
    ReferenceHolder<SocketConnectPollOperation> connect_op(
        new SocketConnectPollOperation(xsink, false, target_str, sock_priv_raw), xsink);
    if (*xsink) {
        return -1;
    }

    // 4. Create the Http1ClientPollOperationPriv (no self yet — set later).
    //    The priv takes ownership of one ref on sock_priv_raw and one ref on
    //    connect_op.
    sock_priv_raw->ref();
    SocketConnectPollOperation* connect_ptr = connect_op.release();
    ReferenceHolder<Http1ClientPollOperationPriv> priv_holder(
        new Http1ClientPollOperationPriv(
            /* self */ nullptr,
            /* sock */ sock_priv_raw,
            /* connect_op */ connect_ptr,
            /* ssl_required */ ssl_required,
            /* proxy_tunnel */ false,
            /* is_proxy_plain */ false,
            /* target_host */ target_host,
            /* target_port */ target_port,
            /* conn_priv */ this),
        xsink);
    Http1ClientPollOperationPriv* priv_raw = *priv_holder;

    // 5. Wrap the priv in a QoreObject.
    ReferenceHolder<QoreObject> poll_obj_holder(
        new QoreObject(QC_HTTP1CLIENTPOLLOPERATIONBASE, pgm, priv_holder.release()), xsink);

    // 6. Set up self references: the poll op priv needs its own self, and the
    //    connect op needs to know about the poll op's QoreObject to build
    //    SocketPollInfo hashes with the right "sock" member.
    priv_raw->setSelf(*poll_obj_holder);
    connect_ptr->setSelf(*poll_obj_holder);

    // 7. Set poll op QoreObject members: "sock" (the Socket QoreObject) and
    //    "goal" — same as the QPP binding would.
    poll_obj_holder->setValue("sock", sock_obj_holder->refSelf(), xsink);
    if (*xsink) {
        return -1;
    }
    poll_obj_holder->setValue("goal", new QoreStringNode("http1_client"), xsink);
    if (*xsink) {
        return -1;
    }

    // 8. Submit to the global AsyncIoController.  The controller needs a
    //    SocketPollOperationInfo hash with sock, spop, owner, and to fields.
    //    We use a process-unique owner string based on our pointer so
    //    cancelByOwner can be used for cleanup.
    ReferenceHolder<QoreObject> ctl_obj_holder(
        qore_get_async_io_controller_obj(xsink), xsink);
    if (*xsink || !ctl_obj_holder) {
        return -1;
    }
    ReferenceHolder<AsyncIoControllerPriv> ctl_priv_holder(
        static_cast<AsyncIoControllerPriv*>(
            ctl_obj_holder->getReferencedPrivateData(CID_ASYNCIOCONTROLLER, xsink)),
        xsink);
    if (*xsink || !ctl_priv_holder) {
        if (!*xsink) {
            xsink->raiseException("HTTPCLIENT-CONNECT-ERROR",
                "failed to get AsyncIoController singleton");
        }
        return -1;
    }

    // Owner string: caller-supplied (via setOwner before construction
    // wired into a manager — Phase P3+) or per-instance default for
    // standalone use (Phase P2 unit tests).
    char owner_buf[64];
    const char* owner_to_use;
    if (!owner_str.empty()) {
        owner_to_use = owner_str.c_str();
    } else {
        snprintf(owner_buf, sizeof(owner_buf), "http1-cpp-conn-%p", (void*)this);
        owner_to_use = owner_buf;
    }

    ReferenceHolder<QoreHashNode> info(new QoreHashNode(autoTypeInfo), xsink);
    info->setKeyValue("sock", sock_obj_holder->refSelf(), xsink);
    info->setKeyValue("spop", poll_obj_holder->refSelf(), xsink);
    info->setKeyValue("owner", new QoreStringNode(owner_to_use), xsink);
    // No per-op timeout: the poll op itself drives the connect timeout and
    // per-request timeouts are handled by q_future_get_blocking on the
    // caller's side.
    info->setKeyValue("to", DateTimeNode::makeRelative(0, 0, 0, 0, 0, 0, 0), xsink);
    if (*xsink) {
        return -1;
    }

    ReferenceHolder<QoreObject> submit_rv(
        ctl_priv_holder->submit(*ctl_obj_holder, *info, false, xsink), xsink);
    if (*xsink) {
        return -1;
    }

    // Success — commit ownership to member variables.  After this point the
    // destructor will manage the lifetimes of these objects.
    sock_priv = sock_priv_raw;
    sock_obj = sock_obj_holder.release();
    poll_op_priv = priv_raw;
    poll_op_obj = poll_obj_holder.release();
    submitted_to_controller = true;
    return 0;
}

int Http1ClientConnection::getActiveStreamCount() const {
    if (!poll_op_priv) {
        return 0;
    }
    return poll_op_priv->getActiveStreamCount();
}

QoreHashNode* Http1ClientConnection::submitRequest(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        ExceptionSink* xsink) {
    if (!poll_op_priv || isClosed()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit request: connection is closed");
        return nullptr;
    }
    if (!isReady()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit request: connection is not ready "
            "(call waitForReadyOrError first)");
        return nullptr;
    }

    // Create Promise + Future + wrap in FutureImpl QoreObject.
    ReferenceHolder<QorePromise> promise_holder(new QorePromise(), xsink);
    QorePromise* promise_raw = *promise_holder;
    ReferenceHolder<QoreFuture> future_holder(promise_holder->getFuture(xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    QoreProgram* pgm = getProgram();
    ReferenceHolder<QoreObject> future_obj(
        new QoreObject(QC_FUTUREIMPL, pgm, future_holder.release()), xsink);

    // Build the PromiseAction.  The ctor refs the promise internally; the
    // caller (this fn) still holds a ref via promise_holder.  Ownership
    // of promise_holder's ref stays with us and gets released at scope
    // exit — the action keeps its own ref.
    PromiseAction* action = new PromiseAction(promise_raw, /* promise_obj */ nullptr);

    // Submit via the poll op.  submitRequest consumes the action's ref on
    // failure; on success it stores the action internally.  max_streams=1
    // for HTTP/1.1.
    int64_t stream_id = poll_op_priv->submitRequest(method, path, headers,
        body, body_len, /* streaming */ false, action, /* max_streams */ 1, xsink);
    if (*xsink || stream_id < 0) {
        // Action was deref'd by submitRequest on failure.
        return nullptr;
    }

    // Wake the I/O controller so it processes the queued request promptly.
    if (sock_obj) {
        ExceptionSink wake_xsink;
        ReferenceHolder<QoreObject> ctl_obj_holder(
            qore_get_async_io_controller_obj(&wake_xsink), &wake_xsink);
        if (ctl_obj_holder) {
            ReferenceHolder<AsyncIoControllerPriv> ctl_priv_holder(
                static_cast<AsyncIoControllerPriv*>(
                    ctl_obj_holder->getReferencedPrivateData(
                        CID_ASYNCIOCONTROLLER, &wake_xsink)),
                &wake_xsink);
            if (ctl_priv_holder) {
                ctl_priv_holder->wakeSocketByObject(sock_obj, &wake_xsink);
            }
        }
        wake_xsink.clear();
    }

    // Return the result hash {stream_id, future}.
    //
    // The PromiseAction owns its own QorePromise ref (taken in its
    // constructor), so the Promise stays alive until the I/O thread runs
    // execute()/executeError() and the action is deref'd by the poll op's
    // notifyPendingStreams() / cleanup().  Therefore the caller does not
    // need to hold a separate Promise reference — only the Future, which
    // is what they pass to q_future_get_blocking().
    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);
    result->setKeyValue("stream_id", QoreValue((int64)stream_id), xsink);
    result->setKeyValue("future", future_obj.release(), xsink);
    // Drop our local Promise ref — the action's ref keeps it alive.
    promise_holder.release()->deref(xsink);
    return result.release();
}

void Http1ClientConnection::closeConnection(ExceptionSink* xsink) {
    if (!poll_op_priv || isClosed()) {
        return;
    }

    // Abort the poll op — transitions priv to CLOSED, notifies pending
    // actions with HTTP1-ABORT, clears our connection_priv raw pointer
    // inside the priv (so the priv no longer dereferences us).
    poll_op_priv->abort(xsink);

    // Cancel the op in the global AsyncIoController so it's removed from
    // the cache and the socket fd released.
    if (submitted_to_controller && sock_priv) {
        ExceptionSink cancel_xsink;
        ReferenceHolder<QoreObject> ctl_obj_holder(
            qore_get_async_io_controller_obj(&cancel_xsink), &cancel_xsink);
        if (ctl_obj_holder) {
            ReferenceHolder<AsyncIoControllerPriv> ctl_priv_holder(
                static_cast<AsyncIoControllerPriv*>(
                    ctl_obj_holder->getReferencedPrivateData(
                        CID_ASYNCIOCONTROLLER, &cancel_xsink)),
                &cancel_xsink);
            if (ctl_priv_holder) {
                ctl_priv_holder->cancel(sock_priv, &cancel_xsink);
            }
        }
        cancel_xsink.clear();
        submitted_to_controller = false;
    }

    // Drive the connection base state machine to CLOSED so any
    // waitForReadyOrError sleeper wakes up.
    setClosed();
}

QoreHashNode* Http1ClientConnection::getReferencedErrorInfo() {
    if (!poll_op_priv) {
        return nullptr;
    }
    return poll_op_priv->getErrorInfo();
}

void Http1ClientConnection::setManager(HttpClientConnectionManagerBase* mgr) {
    AutoLocker al(onclose_lock);
    manager_ = mgr;
}

void Http1ClientConnection::onClosedHook() {
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

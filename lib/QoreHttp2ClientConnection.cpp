/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHttp2ClientConnection.cpp

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
#include "qore/intern/QoreHttp2ClientConnection.h"
#include "qore/intern/QC_Http2ClientPollOperationBase.h"
#include "qore/intern/QC_SocketPollOperation.h"
#include "qore/intern/QC_Socket.h"
#include "qore/intern/QC_FutureImpl.h"
#include "qore/intern/AsyncIoControllerPriv.h"

#include <cstdio>

// QPP-generated symbols are #include'd later in the umbrella
// single-compilation-unit.cpp; forward-declare them here so we can
// reference them.
extern QoreClass* QC_HTTP2CLIENTPOLLOPERATIONBASE;
extern qore_classid_t CID_HTTP2CLIENTPOLLOPERATIONBASE;

Http2ClientConnection::Http2ClientConnection(const char* target_host, int target_port,
        bool ssl_required, int max_concurrent_streams, ExceptionSink* xsink)
    : HttpClientConnectionBase(target_host, target_port, ssl_required),
      max_concurrent_streams_(max_concurrent_streams) {
    if (buildAndSubmit(xsink)) {
        // Constructor failure: holders unwound, member pointers stay
        // nullptr, destructor will be a no-op.
        return;
    }
}

Http2ClientConnection::~Http2ClientConnection() {
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
    poll_op_priv = nullptr;
    sock_priv = nullptr;
    xsink.clear();
}

int Http2ClientConnection::buildAndSubmit(ExceptionSink* xsink) {
    QoreProgram* pgm = getProgram();

    // 1. Create the socket QoreSocketObject + QoreObject wrapper.
    ReferenceHolder<QoreSocketObject> sock_priv_holder(new QoreSocketObject, xsink);
    QoreSocketObject* sock_priv_raw = *sock_priv_holder;

    // 1a. Configure ALPN "h2" BEFORE the connect runs (HTTPS only).  This
    //     is required so the SSL handshake negotiates HTTP/2.  For plain
    //     HTTP h2c, ALPN is not used; the server confirms h2 via SETTINGS.
    if (ssl_required) {
        ReferenceHolder<QoreListNode> protocols(new QoreListNode(autoTypeInfo), xsink);
        protocols->push(new QoreStringNode("h2"), xsink);
        if (*xsink) {
            return -1;
        }
        sock_priv_raw->setAlpnProtocols(*protocols, xsink);
        if (*xsink) {
            return -1;
        }
    }

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

    // 3. Create the SocketConnectPollOperation.  The ctor takes an
    //    already-ref'd QoreSocketObject pointer and adopts it.
    sock_priv_raw->ref();
    ReferenceHolder<SocketConnectPollOperation> connect_op(
        new SocketConnectPollOperation(xsink, false, target_str, sock_priv_raw), xsink);
    if (*xsink) {
        return -1;
    }

    // 4. Create the Http2ClientPollOperationPriv.  Note: H2 priv constructor
    //    has no `is_proxy_plain` parameter (H2 only does HTTPS through
    //    proxy via CONNECT, not plain absolute URI mode).
    sock_priv_raw->ref();
    SocketConnectPollOperation* connect_ptr = connect_op.release();
    ReferenceHolder<Http2ClientPollOperationPriv> priv_holder(
        new Http2ClientPollOperationPriv(
            /* self */ nullptr,
            /* sock */ sock_priv_raw,
            /* connect_op */ connect_ptr,
            /* ssl_required */ ssl_required,
            /* proxy_tunnel */ false,
            /* target_host */ target_host,
            /* target_port */ target_port,
            /* conn_priv */ this),
        xsink);
    Http2ClientPollOperationPriv* priv_raw = *priv_holder;

    // 5. Wrap the priv in a QoreObject.
    ReferenceHolder<QoreObject> poll_obj_holder(
        new QoreObject(QC_HTTP2CLIENTPOLLOPERATIONBASE, pgm, priv_holder.release()), xsink);

    // 6. Set up self references.
    priv_raw->setSelf(*poll_obj_holder);
    connect_ptr->setSelf(*poll_obj_holder);

    // 7. Set poll op QoreObject members: "sock" and "goal" — same as
    //    the QPP binding would.
    poll_obj_holder->setValue("sock", sock_obj_holder->refSelf(), xsink);
    if (*xsink) {
        return -1;
    }
    poll_obj_holder->setValue("goal", new QoreStringNode("http2_client"), xsink);
    if (*xsink) {
        return -1;
    }

    // 8. Submit to the global AsyncIoController.
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

    char owner_buf[64];
    const char* owner_to_use;
    if (!owner_str.empty()) {
        owner_to_use = owner_str.c_str();
    } else {
        snprintf(owner_buf, sizeof(owner_buf), "http2-cpp-conn-%p", (void*)this);
        owner_to_use = owner_buf;
    }

    ReferenceHolder<QoreHashNode> info(new QoreHashNode(autoTypeInfo), xsink);
    info->setKeyValue("sock", sock_obj_holder->refSelf(), xsink);
    info->setKeyValue("spop", poll_obj_holder->refSelf(), xsink);
    info->setKeyValue("owner", new QoreStringNode(owner_to_use), xsink);
    info->setKeyValue("to", DateTimeNode::makeRelative(0, 0, 0, 0, 0, 0, 0), xsink);
    if (*xsink) {
        return -1;
    }

    ReferenceHolder<QoreObject> submit_rv(
        ctl_priv_holder->submit(*ctl_obj_holder, *info, false, xsink), xsink);
    if (*xsink) {
        return -1;
    }

    // Success — commit ownership to member variables atomically.
    sock_priv = sock_priv_raw;
    sock_obj = sock_obj_holder.release();
    poll_op_priv = priv_raw;
    poll_op_obj = poll_obj_holder.release();
    submitted_to_controller = true;
    return 0;
}

int Http2ClientConnection::getActiveStreamCount() const {
    if (!poll_op_priv) {
        return 0;
    }
    return poll_op_priv->getActiveStreamCount();
}

QoreHashNode* Http2ClientConnection::submitRequest(const char* method, const char* path,
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

    // Promise + Future + FutureImpl QoreObject (same pattern as H1).
    ReferenceHolder<QorePromise> promise_holder(new QorePromise(), xsink);
    QorePromise* promise_raw = *promise_holder;
    ReferenceHolder<QoreFuture> future_holder(promise_holder->getFuture(xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    QoreProgram* pgm = getProgram();
    ReferenceHolder<QoreObject> future_obj(
        new QoreObject(QC_FUTUREIMPL, pgm, future_holder.release()), xsink);

    PromiseAction* action = new PromiseAction(promise_raw, /* promise_obj */ nullptr);

    int64_t stream_id = poll_op_priv->submitRequest(method, path, headers,
        body, body_len, /* streaming */ false, action,
        /* max_streams */ max_concurrent_streams_, xsink);
    if (*xsink || stream_id < 0) {
        // Action was deref'd by submitRequest on failure.
        return nullptr;
    }

    // Successful submit: convert any prior reservation into an active
    // stream — same as H1.
    releaseStreamReservation();

    // Wake the I/O controller so it processes the queued request.
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

    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);
    result->setKeyValue("stream_id", QoreValue((int64)stream_id), xsink);
    result->setKeyValue("future", future_obj.release(), xsink);
    promise_holder.release()->deref(xsink);
    return result.release();
}

void Http2ClientConnection::closeConnection(ExceptionSink* xsink) {
    if (!poll_op_priv || isClosed()) {
        return;
    }

    // Cancel the op in the global AsyncIoController — this synchronously
    // waits until the I/O thread stops processing the operation.  The I/O
    // thread's cancel processing calls abort() on the poll op via
    // doCancelIntern → callAbort, so we must NOT call abort() again here.
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
    } else {
        // Not submitted to the I/O controller — abort directly.
        poll_op_priv->abort(xsink);
    }

    setClosed();
}

QoreHashNode* Http2ClientConnection::getReferencedErrorInfo() {
    if (!poll_op_priv) {
        return nullptr;
    }
    return poll_op_priv->getErrorInfo();
}

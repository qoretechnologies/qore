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
#include "qore/intern/QoreChannel.h"
#include "qore/intern/QoreHttp2ClientConnection.h"
#include "qore/intern/QC_Http2ClientPollOperationBase.h"
#include "qore/intern/QC_SocketPollOperation.h"
#include "qore/intern/QC_Socket.h"
#include "qore/intern/QoreObjectIntern.h"
#include "qore/intern/QC_FutureImpl.h"
#include "qore/intern/AsyncIoControllerPriv.h"

#include <cstdio>

// QPP-generated symbols are #include'd later in the umbrella
// single-compilation-unit.cpp; forward-declare them here so we can
// reference them.
extern QoreClass* QC_HTTP2CLIENTPOLLOPERATIONBASE;
extern qore_classid_t CID_HTTP2CLIENTPOLLOPERATIONBASE;

Http2ClientConnection::Http2ClientConnection(const char* target_host, int target_port,
        bool ssl_required, int max_concurrent_streams, ExceptionSink* xsink,
        HttpClientConnectionManagerBase* mgr)
    : HttpClientConnectionBase(target_host, target_port, ssl_required),
      max_concurrent_streams_(max_concurrent_streams) {
    if (mgr) {
        setManager(mgr);
    }
    if (buildAndSubmit(xsink)) {
        // Constructor failure: holders unwound, member pointers stay
        // nullptr, destructor will be a no-op.
        return;
    }
}

Http2ClientConnection::Http2ClientConnection(QoreObject* adopted_sock_obj,
        QoreSocketObject* adopted_sock_priv,
        std::string target_host, int target_port, int max_concurrent_streams,
        ExceptionSink* xsink, HttpClientConnectionManagerBase* mgr)
    : HttpClientConnectionBase(std::move(target_host), target_port,
          /* ssl_required */ true),
      max_concurrent_streams_(max_concurrent_streams) {
    if (mgr) {
        setManager(mgr);
    }
    if (buildAndSubmitAdopted(adopted_sock_obj, adopted_sock_priv, xsink)) {
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

    // Apply TCP_USER_TIMEOUT if the manager configured it.  The value is
    // stored on the socket and re-applied by qore_socket_private at the
    // post-connect hook (confirmConnected).
    if (manager_) {
        int ut_ms = manager_->getOptions().tcp_user_timeout_ms;
        if (ut_ms > 0) {
            sock_priv_raw->setUserTimeout(ut_ms);
        }
    }

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
        new SocketConnectPollOperation(xsink, false, target_str, sock_priv_raw, true), xsink);
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

    // Enable the DGC custom scanner gate for this object — the QPP
    // destructor at QC_Http2ClientPollOperationBase.qpp:1245 always calls
    // decScanPrivateData(), so the scan_private_data counter must be
    // incremented here to balance it.  The QPP constructor does the
    // matching increment on the Qore-level `new Http2ClientPollOperation`
    // path, but that path is NOT taken when we create the QoreObject
    // directly from C++ as above — bypassing the QPP ctor leaves the
    // counter at 0 and the destructor's assertion trips under Debug builds.
    // See commit 53585cdba1 (H2 DGC scanner) for the scanner contract.
    qore_object_private::get(**poll_obj_holder)->incScanPrivateData();

    // 6. Set up self references.
    priv_raw->setSelf(*poll_obj_holder);
    connect_ptr->setSelf(*poll_obj_holder);

    // 7. Finalize: set poll op members, submit to AsyncIoController, and
    //    on success commit the holders to member pointers.  Shared with
    //    buildAndSubmitAdopted — see design/conn-mgr-alpn-negotiation.md §9.
    return finalizePollOpSubmission(sock_obj_holder, poll_obj_holder,
        priv_raw, sock_priv_raw, "http2-cpp-conn-", xsink);
}

int Http2ClientConnection::buildAndSubmitAdopted(QoreObject* adopted_sock_obj,
        QoreSocketObject* adopted_sock_priv, ExceptionSink* xsink) {
    if (!adopted_sock_obj || !adopted_sock_priv) {
        xsink->raiseException("HTTPCLIENT-ADOPT-ERROR",
            "cannot adopt a null socket");
        return -1;
    }
    QoreProgram* pgm = getProgram();
    (void)pgm;

    // Adopt the caller's ref on the existing socket QoreObject wrapper.
    // Do NOT create a new wrapper — see the extended comment in
    // Http1ClientConnection::buildAndSubmitAdopted for why: the close_internal
    // fires on refcount 0 of the wrapper, so a second wrapper over the same
    // priv would race the original to zero and kill the adopted fd before
    // the H2 multiplex op's first continuePoll runs.
    ReferenceHolder<QoreObject> sock_obj_holder(adopted_sock_obj, xsink);
    QoreSocketObject* sock_priv_raw = adopted_sock_priv;

    // Apply TCP_USER_TIMEOUT to the adopted (already-connected) socket if
    // the manager configured it; setUserTimeout applies immediately when
    // the fd is open.
    if (manager_) {
        int ut_ms = manager_->getOptions().tcp_user_timeout_ms;
        if (ut_ms > 0) {
            sock_priv_raw->setUserTimeout(ut_ms);
        }
    }

    // Create the adopt-socket H2 poll op priv.  ssl_required is
    // implicitly true for the adopt path (see design doc §5.1).
    sock_priv_raw->ref();
    ReferenceHolder<Http2ClientPollOperationPriv> priv_holder(
        new Http2ClientPollOperationPriv(
            /* self */ nullptr,
            /* sock */ sock_priv_raw,
            /* target_host */ target_host,
            /* target_port */ target_port,
            /* conn_priv */ this),
        xsink);
    Http2ClientPollOperationPriv* priv_raw = *priv_holder;

    ReferenceHolder<QoreObject> poll_obj_holder(
        new QoreObject(QC_HTTP2CLIENTPOLLOPERATIONBASE, pgm, priv_holder.release()), xsink);

    // Enable the DGC custom scanner gate — see the matching comment in
    // buildAndSubmit() above; the QPP destructor's decScanPrivateData()
    // requires a matching increment when the QoreObject is created
    // directly from C++ instead of via the QPP constructor.
    qore_object_private::get(**poll_obj_holder)->incScanPrivateData();

    priv_raw->setSelf(*poll_obj_holder);

    // Install the H2 multiplex inner op, send the client preface, and
    // transition to READING + ready BEFORE the helper submits to the
    // controller.  Order matters: the I/O thread's first continuePoll()
    // must see h2_state = READING with a live current_op, or
    // handleConnecting would run and error on current_op == nullptr.
    // initAdoptedMultiplex only needs self set — it does not depend on
    // poll_obj members being populated, so running it before the helper
    // (which sets "sock"/"goal") is safe.
    priv_raw->initAdoptedMultiplex(xsink);
    if (*xsink) {
        return -1;
    }

    // Finalize: set poll op members, submit to AsyncIoController, and
    // on success commit the holders to member pointers.  Shared with
    // buildAndSubmit — see design/conn-mgr-alpn-negotiation.md §9.
    return finalizePollOpSubmission(sock_obj_holder, poll_obj_holder,
        priv_raw, sock_priv_raw, "http2-cpp-conn-adopted-", xsink);
}

int Http2ClientConnection::finalizePollOpSubmission(
        ReferenceHolder<QoreObject>& sock_obj_holder,
        ReferenceHolder<QoreObject>& poll_obj_holder,
        Http2ClientPollOperationPriv* priv_raw,
        QoreSocketObject* sock_priv_raw,
        const char* owner_prefix,
        ExceptionSink* xsink) {
    // Set poll op QoreObject members: "sock" and "goal" — same as the
    // QPP binding would.
    poll_obj_holder->setValue("sock", sock_obj_holder->refSelf(), xsink);
    if (*xsink) {
        return -1;
    }
    poll_obj_holder->setValue("goal", new QoreStringNode("http2_client"), xsink);
    if (*xsink) {
        return -1;
    }

    // Get the global AsyncIoController.
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

    // Owner string: caller-supplied (via setOwner) or auto-generated from
    // the supplied prefix + this pointer so cancelByOwner can be used
    // for cleanup.
    char owner_buf[64];
    const char* owner_to_use;
    if (!owner_str.empty()) {
        owner_to_use = owner_str.c_str();
    } else {
        snprintf(owner_buf, sizeof(owner_buf), "%s%p",
            owner_prefix, (void*)this);
        owner_to_use = owner_buf;
    }

    ReferenceHolder<QoreHashNode> info(new QoreHashNode(autoTypeInfo), xsink);
    info->setKeyValue("sock", sock_obj_holder->refSelf(), xsink);
    info->setKeyValue("spop", poll_obj_holder->refSelf(), xsink);
    info->setKeyValue("owner", new QoreStringNode(owner_to_use), xsink);
    // Disable controller-level timeout; the H2 poll op manages its own
    // idle timeout.
    info->setKeyValue("to", -1, xsink);
    if (*xsink) {
        return -1;
    }

    // submit() takes ownership of `info` via the ReferenceHolder in
    // AsyncIoControllerPriv::submit; use .release() not *info.
    ReferenceHolder<QoreObject> submit_rv(
        ctl_priv_holder->submit(*ctl_obj_holder, info.release(), false, xsink), xsink);
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
    MethodGuard g(const_cast<Http2ClientConnection*>(this));
    if (!g.acquired() || !poll_op_priv) {
        return 0;
    }
    return poll_op_priv->getActiveStreamCount();
}

QoreHashNode* Http2ClientConnection::submitRequest(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        ExceptionSink* xsink) {
    MethodGuard g(this);
    if (!g.acquired()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit request: connection has been closed");
        return nullptr;
    }
    if (!poll_op_priv || isClosed()) {
        raiseClosedSubmitError("cannot submit request: connection is closed",
            xsink);
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

int64_t Http2ClientConnection::submitRequestWithAction(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        AbstractAsyncAction* action, ExceptionSink* xsink) {
    MethodGuard g(this);
    if (!g.acquired()) {
        action->deref(xsink);
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit request: connection has been closed");
        return -1;
    }
    if (!poll_op_priv || isClosed()) {
        action->deref(xsink);
        raiseClosedSubmitError("cannot submit request: connection is closed",
            xsink);
        return -1;
    }
    if (!isReady()) {
        action->deref(xsink);
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit request: connection is not ready");
        return -1;
    }

    int64_t stream_id = poll_op_priv->submitRequest(method, path, headers,
        body, body_len, /* streaming */ false, action,
        /* max_streams */ max_concurrent_streams_, xsink);
    if (*xsink || stream_id < 0) {
        // poll_op_priv->submitRequest deref'd the action on failure.
        return -1;
    }

    // Successful submit — release the reserved stream slot (matches H1).
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

    return stream_id;
}

int64_t Http2ClientConnection::submitRequestStreaming(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        QoreChannel*& channel_out, ExceptionSink* xsink) {
    MethodGuard g(this);
    if (!g.acquired()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit streaming request: connection has been closed");
        return -1;
    }
    if (!poll_op_priv || isClosed()) {
        raiseClosedSubmitError(
            "cannot submit streaming request: connection is closed", xsink);
        return -1;
    }
    if (!isReady()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit streaming request: connection is not ready");
        return -1;
    }

    // Create an unbounded Channel for incremental response delivery
    ReferenceHolder<QoreChannel> ch_holder(new QoreChannel(-1), xsink);
    QoreChannel* ch = *ch_holder;

    // Create ChannelAction — poll op takes ownership via submitRequest
    ChannelAction* action = new ChannelAction(ch);

    // streaming=false: Http2ClientPollOperationPriv::submitRequest interprets
    // `streaming` as request-body streaming (caller pushes body via
    // pushSendData), not response streaming.  submitRequestStreaming delivers
    // a one-shot body and only streams the response; passing streaming=true
    // would make the H2 session send HEADERS without END_STREAM and the
    // server would wait indefinitely for DATA frames that never arrive.
    // Response-streaming dispatch is handled via action->isStreaming() in the
    // poll op (setHttp2StreamStreaming).
    int64_t stream_id = poll_op_priv->submitRequest(method, path, headers,
        body, body_len, /* streaming */ false, action,
        /* max_streams */ max_concurrent_streams_, xsink);
    if (*xsink || stream_id < 0) {
        return -1;
    }

    releaseStreamReservation();

    // Wake the I/O controller
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

    // Output the channel to the caller (ref'd; caller must deref)
    ch->ref();
    channel_out = ch;
    return stream_id;
}

QoreHashNode* Http2ClientConnection::submitRequestStreamingSend(const char* method,
        const char* path, const QoreHashNode* headers, bool streaming_recv,
        QoreChannel*& channel_out, ExceptionSink* xsink) {
    MethodGuard g(this);
    if (!g.acquired()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit streaming send request: connection has been closed");
        return nullptr;
    }
    if (!poll_op_priv || isClosed()) {
        raiseClosedSubmitError(
            "cannot submit streaming send request: connection is closed",
            xsink);
        return nullptr;
    }
    if (!isReady()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit streaming send request: connection is not ready");
        return nullptr;
    }

    AbstractAsyncAction* action = nullptr;
    ReferenceHolder<QoreObject> future_obj(xsink);
    ReferenceHolder<QoreChannel> ch_holder(xsink);

    if (streaming_recv) {
        // Streaming receive via Channel
        ch_holder = new QoreChannel(-1);
        action = new ChannelAction(*ch_holder);
    } else {
        // Non-streaming receive via Promise/Future
        ReferenceHolder<QorePromise> promise_holder(new QorePromise(), xsink);
        QorePromise* promise_raw = *promise_holder;
        ReferenceHolder<QoreFuture> future_holder(promise_holder->getFuture(xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        QoreProgram* pgm = getProgram();
        future_obj = new QoreObject(QC_FUTUREIMPL, pgm, future_holder.release());
        action = new PromiseAction(promise_raw, nullptr);
        promise_holder.release()->deref(xsink);
    }

    // Submit with streaming=true (no END_STREAM on headers — bidirectional streaming)
    int64_t stream_id = poll_op_priv->submitRequest(method, path, headers,
        nullptr, 0, /* streaming */ true, action,
        /* max_streams */ max_concurrent_streams_, xsink);
    if (*xsink || stream_id < 0) {
        return nullptr;
    }

    // Store stream_id for subsequent pushSendData/setTrailers calls
    streaming_send_stream_id = stream_id;

    releaseStreamReservation();

    // Wake the I/O controller
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

    // Build result
    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);
    result->setKeyValue("stream_id", QoreValue((int64)stream_id), xsink);
    if (streaming_recv) {
        QoreChannel* ch = *ch_holder;
        ch->ref();
        channel_out = ch;
    } else {
        result->setKeyValue("future", future_obj.release(), xsink);
    }
    return result.release();
}

void Http2ClientConnection::pushSendData(const void* data, size_t len, ExceptionSink* xsink) {
    MethodGuard g(this);
    if (!g.acquired()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot push data: connection has been closed");
        return;
    }
    if (!poll_op_priv) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot push data: connection has no poll operation");
        return;
    }
    if (streaming_send_stream_id < 0) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot push data: no streaming send in progress");
        return;
    }

    bool end_stream = (!data || !len);
    if (end_stream) {
        // End-of-body: send empty DATA with END_STREAM
        poll_op_priv->sendStreamData(streaming_send_stream_id, nullptr, true, xsink);
        streaming_send_stream_id = -1;
    } else {
        // Create BinaryNode from raw data for the H2 sendStreamData API
        // BinaryNode takes ownership of the buffer, so we must copy
        void* buf = malloc(len);
        if (!buf) {
            xsink->raiseException("HTTPCLIENT-MEMORY-ERROR",
                "failed to allocate %zu bytes for stream data", len);
            return;
        }
        memcpy(buf, data, len);
        SimpleRefHolder<BinaryNode> bin(new BinaryNode(buf, len));
        poll_op_priv->sendStreamData(streaming_send_stream_id, *bin, false, xsink);
    }

    // Wake the I/O controller so it processes the queued data
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
}

void Http2ClientConnection::setTrailers(const QoreHashNode* trailers, ExceptionSink* xsink) {
    MethodGuard g(this);
    if (!g.acquired()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot set trailers: connection has been closed");
        return;
    }
    if (!poll_op_priv || !sock_priv) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot set trailers: connection has no poll operation");
        return;
    }
    if (streaming_send_stream_id < 0) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot set trailers: no streaming send in progress");
        return;
    }

    sock_priv->sendHttp2Trailers(streaming_send_stream_id, trailers, xsink);
}

void Http2ClientConnection::closeConnection(ExceptionSink* xsink) {
    // Lifetime barrier (S1): invalidate so new method calls are refused,
    // then wait for any in-flight calls to finish before tearing down
    // protocol-specific state.  See HttpClientConnection.h.
    markInvalidated();
    drainInFlight();

    if (!poll_op_priv) {
        return;
    }

    // Disarm the raw connection_priv back-pointer BEFORE cancel.  The
    // I/O thread's cancel processing calls abort() which reads
    // connection_priv — without this disarm, abort() can dereference a
    // destroyed connection if the app thread destroys us after cancel
    // returns (the H2 cancel-abort race — see project_h2_cancel_race.md).
    poll_op_priv->disarmConnectionPriv();

    // Cancel the op in the global AsyncIoController — this synchronously
    // waits until the I/O thread stops processing the operation.  The I/O
    // thread's cancel processing calls abort() on the poll op via
    // doCancelIntern → callAbort, so we must NOT call abort() again here.
    //
    // NOTE: we must always cancel even if isClosed() is true.  The I/O
    // thread's setError() calls connection_priv->setClosed() (which sets
    // the state to CLOSED) before nulling connection_priv.  If we skip
    // the cancel based on isClosed(), the poll op remains in the I/O
    // controller's cache with a stale connection_priv pointer — the I/O
    // thread would access freed memory when it later processes the entry.
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
    } else if (!isClosed()) {
        // Not submitted to the I/O controller — abort directly.
        poll_op_priv->abort(xsink);
    }

    setClosed();
}

QoreHashNode* Http2ClientConnection::getReferencedErrorInfo() {
    MethodGuard g(this);
    if (!g.acquired() || !poll_op_priv) {
        return nullptr;
    }
    return poll_op_priv->getErrorInfo();
}

void Http2ClientConnection::setIdleTimeoutHook(int64_t timeout_us) {
    MethodGuard g(this);
    if (!g.acquired() || !poll_op_priv) {
        return;
    }
    poll_op_priv->setIdleTimeout(timeout_us);
}

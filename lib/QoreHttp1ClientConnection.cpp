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
#include "qore/intern/QoreChannel.h"
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
        bool ssl_required, ExceptionSink* xsink,
        HttpClientConnectionManagerBase* mgr,
        const Http1SslConfig& ssl_config)
    : HttpClientConnectionBase(target_host, target_port, ssl_required) {
    // Apply SSL config BEFORE buildAndSubmit so the socket has the correct
    // settings when the I/O thread starts the SSL handshake.
    ssl_verify_mode = ssl_config.verify_mode;
    accept_all_certs = ssl_config.accept_all;
    client_cert = ssl_config.cert;
    client_key = ssl_config.key;
    // Set the manager BEFORE submitting to the I/O controller so that
    // onClosedHook dispatches correctly even when the I/O thread
    // processes a connect failure before we return to the caller.
    if (mgr) {
        setManager(mgr);
    }
    if (buildAndSubmit(xsink)) {
        // buildAndSubmit raised an exception and left the partial state
        // clean (all refs released).  Caller must still deref this object.
        return;
    }
}

Http1ClientConnection::Http1ClientConnection(const char* target_host, int target_port,
        bool ssl_required, const char* proxy_host, int proxy_port,
        ExceptionSink* xsink, HttpClientConnectionManagerBase* mgr,
        const Http1SslConfig& ssl_config)
    : HttpClientConnectionBase(target_host, target_port, ssl_required),
      proxy_host(proxy_host), proxy_port(proxy_port) {
    ssl_verify_mode = ssl_config.verify_mode;
    accept_all_certs = ssl_config.accept_all;
    client_cert = ssl_config.cert;
    client_key = ssl_config.key;
    if (mgr) {
        setManager(mgr);
    }
    if (buildAndSubmit(xsink)) {
        return;
    }
}

Http1ClientConnection::Http1ClientConnection(QoreSocketObject* adopted_sock,
        std::string target_host, int target_port, bool ssl_required,
        ExceptionSink* xsink, HttpClientConnectionManagerBase* mgr)
    : HttpClientConnectionBase(std::move(target_host), target_port, ssl_required) {
    // Register the manager back-pointer BEFORE submitting so onClosedHook
    // dispatches correctly even if the I/O thread fires a close event
    // before the constructor returns.
    if (mgr) {
        setManager(mgr);
    }
    if (buildAndSubmitAdopted(adopted_sock, xsink)) {
        return;
    }
}

void Http1ClientConnection::configureSsl(int verify_mode, bool accept_all,
        QoreSSLCertificate* cert, QoreSSLPrivateKey* key) {
    ssl_verify_mode = verify_mode;
    accept_all_certs = accept_all;
    client_cert = cert;
    client_key = key;
    // Apply to the socket immediately — safe to call while the TCP connect
    // is in progress because SSL settings only take effect during the
    // handshake (which starts after TCP connect completes).  The poll op's
    // continuePoll on the I/O thread reads these from the socket under its
    // own state lock, so there's no data race.
    if (sock_priv) {
        sock_priv->setSslVerifyMode(verify_mode);
        sock_priv->acceptAllCertificates(accept_all);
        if (cert) {
            cert->ref();
            sock_priv->setCertificate(cert);
        }
        if (key) {
            key->ref();
            sock_priv->setPrivateKey(key);
        }
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

    // 1b. Apply SSL configuration to the socket before connect.
    //     These settings take effect when the poll op starts the SSL handshake
    //     (after TCP connect or after CONNECT tunnel for proxied HTTPS).
    //     The fields are set either by direct member init (standalone use) or
    //     by configureSsl() called from the manager after construction.
    sock_priv_raw->setSslVerifyMode(ssl_verify_mode);
    sock_priv_raw->acceptAllCertificates(accept_all_certs);
    if (client_cert) {
        client_cert->ref();
        sock_priv_raw->setCertificate(client_cert);
    }
    if (client_key) {
        client_key->ref();
        sock_priv_raw->setPrivateKey(client_key);
    }

    // 2. Build the TCP connect target string "host:port".
    //    When a proxy is configured, we connect to the proxy, not the target.
    bool use_proxy = !proxy_host.empty();
    bool use_proxy_tunnel = use_proxy && ssl_required;
    bool use_proxy_plain = use_proxy && !ssl_required;

    const char* connect_host = use_proxy ? proxy_host.c_str() : target_host.c_str();
    int connect_port = use_proxy ? proxy_port : target_port;

    char target_str[256];
    int n = snprintf(target_str, sizeof(target_str), "%s:%d", connect_host, connect_port);
    if (n <= 0 || (size_t)n >= sizeof(target_str)) {
        xsink->raiseException("HTTPCLIENT-CONNECT-ERROR",
            "connect target host:port string too long: host='%s' port=%d",
            connect_host, connect_port);
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
    //    proxy_tunnel=true when HTTPS through proxy (triggers CONNECT ladder)
    //    is_proxy_plain=true when plain HTTP through proxy (uses absolute URIs)
    sock_priv_raw->ref();
    SocketConnectPollOperation* connect_ptr = connect_op.release();
    ReferenceHolder<Http1ClientPollOperationPriv> priv_holder(
        new Http1ClientPollOperationPriv(
            /* self */ nullptr,
            /* sock */ sock_priv_raw,
            /* connect_op */ connect_ptr,
            /* ssl_required */ ssl_required,
            /* proxy_tunnel */ use_proxy_tunnel,
            /* is_proxy_plain */ use_proxy_plain,
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
    // Disable the controller-level timeout for this poll op.  The H1 poll
    // op manages its own idle timeout via armIdleDeadline(), and per-request
    // timeouts are handled by q_future_get_blocking on the caller's side.
    // A negative "to" value tells the controller to never expire this entry.
    info->setKeyValue("to", -1, xsink);
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

int Http1ClientConnection::buildAndSubmitAdopted(QoreSocketObject* adopted_sock_priv,
        ExceptionSink* xsink) {
    if (!adopted_sock_priv) {
        xsink->raiseException("HTTPCLIENT-ADOPT-ERROR",
            "cannot adopt a null socket");
        return -1;
    }
    QoreProgram* pgm = getProgram();

    // Adopt the caller's ref on adopted_sock_priv by handing it directly to
    // a ReferenceHolder; on any fallible step after this, the holder
    // unwinds and the ref is released.
    ReferenceHolder<QoreSocketObject> sock_priv_holder(adopted_sock_priv, xsink);
    QoreSocketObject* sock_priv_raw = *sock_priv_holder;
    ReferenceHolder<QoreObject> sock_obj_holder(
        new QoreObject(QC_SOCKET, pgm, sock_priv_holder.release()), xsink);

    // Create the adopt-socket H1 poll op priv.  It owns one ref on
    // sock_priv_raw (we bump the ref here because sock_obj holds the
    // original) and starts in CONNECTING with no current_op.
    sock_priv_raw->ref();
    ReferenceHolder<Http1ClientPollOperationPriv> priv_holder(
        new Http1ClientPollOperationPriv(
            /* self */ nullptr,
            /* sock */ sock_priv_raw,
            /* ssl_required */ ssl_required,
            /* target_host */ target_host,
            /* target_port */ target_port,
            /* conn_priv */ this),
        xsink);
    Http1ClientPollOperationPriv* priv_raw = *priv_holder;

    ReferenceHolder<QoreObject> poll_obj_holder(
        new QoreObject(QC_HTTP1CLIENTPOLLOPERATIONBASE, pgm, priv_holder.release()), xsink);

    priv_raw->setSelf(*poll_obj_holder);

    // Set poll op QoreObject members: "sock" and "goal" — same as the
    // connect-path constructor does.
    poll_obj_holder->setValue("sock", sock_obj_holder->refSelf(), xsink);
    if (*xsink) {
        return -1;
    }
    poll_obj_holder->setValue("goal", new QoreStringNode("http1_client"), xsink);
    if (*xsink) {
        return -1;
    }

    // Transition the priv to READING and fire the ready callback.  This
    // brings the C++ connection state to READY before we submit to the
    // controller, so a waitForReadyOrError from the caller completes
    // immediately.
    priv_raw->initAdoptedReady(xsink);
    if (*xsink) {
        return -1;
    }

    // Submit the poll op to the AsyncIoController.  Identical to
    // buildAndSubmit's submission tail.
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
            xsink->raiseException("HTTPCLIENT-ADOPT-ERROR",
                "failed to get AsyncIoController singleton");
        }
        return -1;
    }

    char owner_buf[64];
    const char* owner_to_use;
    if (!owner_str.empty()) {
        owner_to_use = owner_str.c_str();
    } else {
        snprintf(owner_buf, sizeof(owner_buf),
            "http1-cpp-conn-adopted-%p", (void*)this);
        owner_to_use = owner_buf;
    }

    ReferenceHolder<QoreHashNode> info(new QoreHashNode(autoTypeInfo), xsink);
    info->setKeyValue("sock", sock_obj_holder->refSelf(), xsink);
    info->setKeyValue("spop", poll_obj_holder->refSelf(), xsink);
    info->setKeyValue("owner", new QoreStringNode(owner_to_use), xsink);
    info->setKeyValue("to", -1, xsink);
    if (*xsink) {
        return -1;
    }

    ReferenceHolder<QoreObject> submit_rv(
        ctl_priv_holder->submit(*ctl_obj_holder, *info, false, xsink), xsink);
    if (*xsink) {
        return -1;
    }

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
        body, body_len, /* streaming */ false, action, /* max_streams */ 1,
        /* streaming_send */ false, xsink);
    if (*xsink || stream_id < 0) {
        // Action was deref'd by submitRequest on failure.  The caller
        // (manager) is responsible for releasing any prior stream
        // reservation it might have made via tryReserveStream.
        return nullptr;
    }

    // Successful submit: convert any prior reservation into an active
    // stream.  The poll op already incremented its own active stream
    // count atomically inside submitRequest, so we just decrement the
    // pending count here.  No-op if no manager reserved a slot.
    releaseStreamReservation();

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

int64_t Http1ClientConnection::submitRequestStreaming(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        QoreChannel*& channel_out, ExceptionSink* xsink) {
    if (!poll_op_priv || isClosed()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit streaming request: connection is closed");
        return -1;
    }
    if (!isReady()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit streaming request: connection is not ready");
        return -1;
    }

    // Create an unbounded Channel for incremental response delivery.
    ReferenceHolder<QoreChannel> ch_holder(new QoreChannel(-1), xsink);
    QoreChannel* ch = *ch_holder;

    // Create ChannelAction — poll op takes ownership via submitRequest.
    ChannelAction* action = new ChannelAction(ch);

    // Submit with streaming=true so the poll op uses
    // dispatchStreamingHeaders/Data/End instead of dispatchResponse.
    int64_t stream_id = poll_op_priv->submitRequest(method, path, headers,
        body, body_len, /* streaming */ true, action, /* max_streams */ 1,
        /* streaming_send */ false, xsink);
    if (*xsink || stream_id < 0) {
        return -1;
    }

    releaseStreamReservation();

    // Wake the I/O controller.
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

    // Output the channel to the caller (ref'd; caller must deref).
    ch->ref();
    channel_out = ch;
    return stream_id;
}

QoreHashNode* Http1ClientConnection::submitRequestStreamingSend(const char* method,
        const char* path, const QoreHashNode* headers, bool streaming_recv,
        QoreChannel*& channel_out, ExceptionSink* xsink) {
    if (!poll_op_priv || isClosed()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit streaming send request: connection is closed");
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

    // Submit with streaming_send=true
    int64_t stream_id = poll_op_priv->submitRequest(method, path, headers,
        nullptr, 0, /* streaming */ streaming_recv, action, /* max_streams */ 1,
        /* streaming_send */ true, xsink);
    if (*xsink || stream_id < 0) {
        return nullptr;
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

void Http1ClientConnection::pushSendData(const QoreStringNode* data, ExceptionSink* xsink) {
    if (!poll_op_priv) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot push data: connection has no poll operation");
        return;
    }
    poll_op_priv->pushSendData(data, xsink);

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

void Http1ClientConnection::setTrailers(const QoreHashNode* trailers, ExceptionSink* xsink) {
    if (!poll_op_priv) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot set trailers: connection has no poll operation");
        return;
    }
    poll_op_priv->setTrailers(trailers, xsink);
}

int64_t Http1ClientConnection::submitRequestWithAction(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        AbstractAsyncAction* action, ExceptionSink* xsink) {
    if (!poll_op_priv || isClosed()) {
        action->deref(xsink);
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit request: connection is closed");
        return -1;
    }
    if (!isReady()) {
        action->deref(xsink);
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit request: connection is not ready");
        return -1;
    }

    int64_t stream_id = poll_op_priv->submitRequest(method, path, headers,
        body, body_len, /* streaming */ false, action, /* max_streams */ 1,
        /* streaming_send */ false, xsink);
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

    return stream_id;
}

void Http1ClientConnection::closeConnection(ExceptionSink* xsink) {
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

/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHttp3ClientConnection.cpp

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
#include "qore/intern/QoreHttp3ClientConnection.h"
#include "qore/intern/QC_Http3ClientPollOperationBase.h"
#include "qore/intern/QC_SocketPollOperation.h"
#include "qore/intern/QC_Socket.h"
#include "qore/intern/AsyncIoControllerPriv.h"

#include <cstdio>

#include <sys/socket.h>
#include <netdb.h>

// QPP-generated symbols — forward-declared for the single-compilation-unit
extern QoreClass* QC_HTTP3CLIENTPOLLOPERATIONBASE;
extern qore_classid_t CID_HTTP3CLIENTPOLLOPERATIONBASE;

//! Resolve address family from hostname to determine IPv4 vs IPv6 for
//! the UDP bind.  Mirrors Http3ClientPollOperation::resolveAddressFamily.
static int resolveAddressFamily(const char* host) {
    struct addrinfo hints{};
    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_family = AF_UNSPEC;
    struct addrinfo* res = nullptr;
    int rc = getaddrinfo(host, nullptr, &hints, &res);
    if (rc == 0 && res) {
        int family = res->ai_family;
        freeaddrinfo(res);
        return family;
    }
    if (res) {
        freeaddrinfo(res);
    }
    // Not a numeric address — try full resolution to determine family
    hints.ai_flags = 0;
    rc = getaddrinfo(host, nullptr, &hints, &res);
    if (rc == 0 && res) {
        int family = res->ai_family;
        freeaddrinfo(res);
        return family;
    }
    if (res) {
        freeaddrinfo(res);
    }
    return AF_INET;  // Default to IPv4
}

Http3ClientConnection::Http3ClientConnection(const char* target_host, int target_port,
        int max_concurrent_streams, ExceptionSink* xsink)
    : HttpClientConnectionBase(target_host, target_port, /* ssl_required (always for H3) */ true),
      max_concurrent_streams_(max_concurrent_streams) {
    if (buildAndSubmit(xsink)) {
        return;
    }
}

Http3ClientConnection::~Http3ClientConnection() {
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

int Http3ClientConnection::buildAndSubmit(ExceptionSink* xsink) {
    QoreProgram* pgm = getProgram();

    // 1. Create UDP socket.
    ReferenceHolder<QoreSocketObject> sock_priv_holder(new QoreSocketObject, xsink);
    QoreSocketObject* sock_priv_raw = *sock_priv_holder;

    // 1a. Bind UDP socket to an ephemeral port.  Address family is
    //     determined from the target hostname (IPv4 vs IPv6).
    int family = resolveAddressFamily(target_host.c_str());
    const char* bind_addr = (family == AF_INET6) ? "::" : "0.0.0.0";
    int bind_rc = sock_priv_raw->bindINET(bind_addr, "0", true, family, SOCK_DGRAM,
        0, xsink);
    if (bind_rc < 0 || *xsink) {
        if (!*xsink) {
            xsink->raiseException("HTTPCLIENT-CONNECT-ERROR",
                "failed to bind UDP socket for QUIC connection to %s:%d",
                target_host.c_str(), target_port);
        }
        return -1;
    }

    ReferenceHolder<QoreObject> sock_obj_holder(
        new QoreObject(QC_SOCKET, pgm, sock_priv_holder.release()), xsink);

    // 2. Create the SocketQuicClientPollOperation (QUIC handshake inner op).
    sock_priv_raw->ref();
    ReferenceHolder<SocketQuicClientPollOperation> inner_op(
        new SocketQuicClientPollOperation(xsink, sock_priv_raw,
            target_host.c_str(), static_cast<uint16_t>(target_port), family),
        xsink);
    if (*xsink) {
        return -1;
    }

    // 3. Create the Http3ClientPollOperationPriv.
    sock_priv_raw->ref();
    SocketQuicClientPollOperation* inner_ptr = inner_op.release();
    ReferenceHolder<Http3ClientPollOperationPriv> priv_holder(
        new Http3ClientPollOperationPriv(
            /* self */ nullptr,
            /* sock */ sock_priv_raw,
            /* inner */ inner_ptr,
            /* conn_priv */ this),
        xsink);
    Http3ClientPollOperationPriv* priv_raw = *priv_holder;

    // 4. Wrap in QoreObject.
    ReferenceHolder<QoreObject> poll_obj_holder(
        new QoreObject(QC_HTTP3CLIENTPOLLOPERATIONBASE, pgm, priv_holder.release()), xsink);

    // 5. Set self references.
    priv_raw->setSelf(*poll_obj_holder);
    inner_ptr->setSelf(*poll_obj_holder);

    // 6. Set QoreObject members.
    poll_obj_holder->setValue("sock", sock_obj_holder->refSelf(), xsink);
    if (*xsink) {
        return -1;
    }
    poll_obj_holder->setValue("goal", new QoreStringNode("http3_client"), xsink);
    if (*xsink) {
        return -1;
    }

    // 7. Submit to controller.
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
        snprintf(owner_buf, sizeof(owner_buf), "http3-cpp-conn-%p", (void*)this);
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

    // Success — commit to members.
    sock_priv = sock_priv_raw;
    sock_obj = sock_obj_holder.release();
    poll_op_priv = priv_raw;
    poll_op_obj = poll_obj_holder.release();
    submitted_to_controller = true;
    return 0;
}

int Http3ClientConnection::getActiveStreamCount() const {
    if (!poll_op_priv) {
        return 0;
    }
    return poll_op_priv->getActiveStreamCount();
}

QoreHashNode* Http3ClientConnection::submitRequest(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        ExceptionSink* xsink) {
    // Phase P5 stub.  HTTP/3 request submission requires a multi-phase
    // protocol that goes through the Qore-language Socket::submitQuicRequest
    // method (QUIC session management + nghttp3 framing).  This cannot be
    // done via direct C++ calls on the Http3ClientPollOperationPriv because
    // the QUIC session state is managed by the Socket layer.
    //
    // Phase P6 will wire the Qore HttpClientIo layer to inherit from the
    // C++ manager base and delegate H3 requests through the existing Qore
    // Http3ClientPollOperation::submitRequest() method.
    xsink->raiseException("HTTP3-NOT-IMPLEMENTED",
        "HTTP/3 request submission via the C++ connection class is not yet "
        "implemented (Phase P5); use the Qore HttpClientIo layer for now");
    return nullptr;
}

void Http3ClientConnection::closeConnection(ExceptionSink* xsink) {
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

QoreHashNode* Http3ClientConnection::getReferencedErrorInfo() {
    if (!poll_op_priv) {
        return nullptr;
    }
    return poll_op_priv->getErrorInfo();
}

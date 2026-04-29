/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    NegotiatingConnectionPollOp.cpp

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
#include "qore/intern/NegotiatingConnectionPollOp.h"
#include "qore/intern/QoreHttp1ClientConnection.h"
#include "qore/intern/QoreHttp2ClientConnection.h"
#include "qore/intern/QC_SocketPollOperation.h"
#include "qore/intern/QC_SocketPollOperationBase.h"
#include "qore/intern/QC_Socket.h"
#include "qore/intern/AsyncIoControllerPriv.h"
#include "qore/intern/QoreAsyncIoLogger.h"

#include <cstdio>

// ============================================================
// NegotiatingConnectionPollOpPriv
// ============================================================

NegotiatingConnectionPollOpPriv::NegotiatingConnectionPollOpPriv(QoreObject* self,
        QoreSocketObject* sock, SocketPollOperationBase* connect_op,
        std::string target_host, int target_port,
        NegotiatingHttpClientConnection* owner_conn)
    : SocketPollOperationBase(self), current_op(connect_op), sock_obj(sock),
      target_host(std::move(target_host)), target_port(target_port),
      owner_conn(owner_conn) {
    // sock and connect_op refs are transferred from the caller.
    int us;
    submit_time_us = q_epoch_us(us) * 1000000LL + us;
    ASYNC_IO_TRACE("negotiate-create target='%s:%d' priv=%p\n",
        this->target_host.c_str(), this->target_port, (void*)this);
    qore_async_io_log(QORE_LOG_LEVEL_DEBUG,
        "negotiate-create target='%s:%d' priv=%p",
        this->target_host.c_str(), this->target_port, (void*)this);
}

NegotiatingConnectionPollOpPriv::~NegotiatingConnectionPollOpPriv() {
    ExceptionSink xsink;
    if (current_op) {
        current_op->deref(&xsink);
        current_op = nullptr;
    }
    if (sock_obj) {
        sock_obj->deref(&xsink);
        sock_obj = nullptr;
    }
    if (error_info) {
        error_info->deref(&xsink);
        error_info = nullptr;
    }
    xsink.clear();
}

void NegotiatingConnectionPollOpPriv::releaseCurrentOp(ExceptionSink* xsink) {
    if (current_op) {
        current_op->deref(xsink);
        current_op = nullptr;
    }
}

const char* NegotiatingConnectionPollOpPriv::getStateImpl() const {
    switch (neg_state.load(std::memory_order_acquire)) {
        case NegState::CONNECTING: return "connecting";
        case NegState::DECIDED:    return "decided";
        case NegState::CLOSED:     return "closed";
    }
    return "unknown";
}

QoreHashNode* NegotiatingConnectionPollOpPriv::continuePoll(ExceptionSink* xsink) {
    while (true) {
        NegState s = neg_state.load(std::memory_order_acquire);
        switch (s) {
            case NegState::CONNECTING:
                return handleConnecting(xsink);
            case NegState::DECIDED:
                // goalReached() returns true for DECIDED, so the
                // controller finalizes the op.  Returning nullptr here
                // signals "no more work".
                return nullptr;
            case NegState::CLOSED: {
                // Raise the stored error so the controller surfaces it.
                if (error_info) {
                    QoreValue err_v = error_info->getKeyValue("err");
                    QoreValue desc_v = error_info->getKeyValue("desc");
                    xsink->raiseException(
                        err_v.getType() == NT_STRING
                            ? err_v.get<const QoreStringNode>()->c_str()
                            : "HTTPCLIENT-NEGOTIATE-ERROR",
                        desc_v.getType() == NT_STRING
                            ? new QoreStringNode(*desc_v.get<const QoreStringNode>())
                            : new QoreStringNode("negotiation failed"));
                }
                return nullptr;
            }
        }
    }
}

QoreHashNode* NegotiatingConnectionPollOpPriv::handleConnecting(ExceptionSink* xsink) {
    int us;
    int64_t now_us = q_epoch_us(us) * 1000000LL + us;
    int64_t elapsed_us = now_us - submit_time_us;
    ASYNC_IO_TRACE("handleConnecting begin target='%s:%d' priv=%p elapsed_us=%lld\n",
        target_host.c_str(), target_port, (void*)this, (long long)elapsed_us);

    if (!current_op) {
        setError("HTTPCLIENT-NEGOTIATE-ERROR",
            "connect operation not initialized", xsink);
        return nullptr;
    }

    ExceptionSink poll_xsink;
    QoreHashNode* poll_info = current_op->continuePoll(&poll_xsink);
    if (poll_xsink) {
        const QoreStringNode* err_str = poll_xsink.getExceptionErr()
            .get<const QoreStringNode>();
        const QoreStringNode* desc_str = poll_xsink.getExceptionDesc()
            .get<const QoreStringNode>();
        setError(
            err_str ? err_str->c_str() : "HTTPCLIENT-NEGOTIATE-ERROR",
            desc_str ? desc_str->c_str() : "TCP+TLS handshake failed",
            xsink);
        poll_xsink.clear();
        return nullptr;
    }

    if (poll_info) {
        // Still progressing — pass the poll info back to the controller
        // so it re-arms epoll for the right events.
        ASYNC_IO_TRACE("handleConnecting still-connecting target='%s:%d' priv=%p elapsed_us=%lld\n",
            target_host.c_str(), target_port, (void*)this, (long long)elapsed_us);
        return poll_info;
    }

    if (!current_op->goalReached()) {
        setError("HTTPCLIENT-NEGOTIATE-ERROR",
            "TCP+TLS handshake ended prematurely", xsink);
        return nullptr;
    }

    ASYNC_IO_TRACE("handleConnecting tcp+tls done target='%s:%d' priv=%p elapsed_us=%lld\n",
        target_host.c_str(), target_port, (void*)this, (long long)elapsed_us);
    qore_async_io_log(QORE_LOG_LEVEL_DEBUG,
        "negotiate-handshake-done target='%s:%d' priv=%p elapsed_us=%lld",
        target_host.c_str(), target_port, (void*)this, (long long)elapsed_us);

    // TCP + SSL handshake done.  Read the negotiated ALPN protocol.
    SimpleRefHolder<QoreStringNode> alpn(sock_obj->getAlpnProtocolForAsyncPoll());
    std::string alpn_id;
    if (alpn && !alpn->empty()) {
        alpn_id = alpn->c_str();
    }

    ASYNC_IO_TRACE("handleConnecting alpn='%s' target='%s:%d' priv=%p elapsed_us=%lld\n",
        alpn_id.c_str(), target_host.c_str(), target_port, (void*)this, (long long)elapsed_us);

    // Validate the ALPN id against our offer list (defense against
    // non-conforming servers — per RFC 7301 the server MUST reject with
    // no_application_protocol if it can't honour the offer, but not all
    // servers are conformant).  An empty ALPN means the server didn't
    // pick one; treat it as http/1.1 (the default for HTTPS without
    // ALPN).
    if (!alpn_id.empty() && alpn_id != "h2" && alpn_id != "http/1.1") {
        std::string desc("unexpected ALPN id negotiated: ");
        desc += alpn_id;
        setError("HTTPCLIENT-NEGOTIATE-ALPN-ERROR", desc.c_str(), xsink);
        return nullptr;
    }

    // Publish the decision to the owning connection (which will read it
    // after waitForReady returns) and fire the ready callback to wake
    // the app thread.  The ownership transition from I/O thread to app
    // thread is ordered by the AbstractHttpPollConnectionPriv condition
    // variable internal to onConnectionReady().
    ASYNC_IO_TRACE("onAlpnDecided fire alpn='%s' target='%s:%d' priv=%p elapsed_us=%lld\n",
        alpn_id.c_str(), target_host.c_str(), target_port, (void*)this,
        (long long)elapsed_us);
    qore_async_io_log(QORE_LOG_LEVEL_DEBUG,
        "negotiate-alpn-decided alpn='%s' target='%s:%d' priv=%p elapsed_us=%lld",
        alpn_id.c_str(), target_host.c_str(), target_port, (void*)this,
        (long long)elapsed_us);
    notifyOwnerReady(std::move(alpn_id));

    // Drop the inner connect op — its dtor clears the socket's
    // non_block_flags, leaving the socket ready to be adopted by an
    // Http1/Http2 connection constructor.
    releaseCurrentOp(xsink);

    // Transition to DECIDED.  The state machine loop will not re-enter
    // handleConnecting — continuePoll() will see DECIDED and return
    // nullptr.  Because needsCloseOnComplete() returns false for
    // DECIDED, the controller finalizes and unregisters without closing
    // the socket.
    neg_state.store(NegState::DECIDED, std::memory_order_release);
    return nullptr;
}

void NegotiatingConnectionPollOpPriv::setError(const char* err, const char* desc,
        ExceptionSink* xsink) {
    if (error_info) {
        return;  // preserve the first error
    }

    int us;
    int64_t elapsed_us = (q_epoch_us(us) * 1000000LL + us) - submit_time_us;
    ASYNC_IO_TRACE("negotiate-setError err='%s' desc='%s' target='%s:%d' priv=%p elapsed_us=%lld\n",
        err, desc, target_host.c_str(), target_port, (void*)this, (long long)elapsed_us);
    qore_async_io_log(QORE_LOG_LEVEL_DEBUG,
        "negotiate-setError err='%s' desc='%s' target='%s:%d' priv=%p elapsed_us=%lld",
        err, desc, target_host.c_str(), target_port, (void*)this, (long long)elapsed_us);

    error_info = new QoreHashNode(autoTypeInfo);
    error_info->setKeyValue("err", new QoreStringNode(err), xsink);
    error_info->setKeyValue("desc", new QoreStringNode(desc), xsink);

    neg_state.store(NegState::CLOSED, std::memory_order_release);

    // Transition the owning connection to CLOSED so the app thread
    // blocked in waitForReadyOrError wakes up and observes the failure.
    // Without this, the waiter would sleep until connect_timeout_ms
    // expires and surface a timeout instead of the real handshake
    // error.  Matches the pattern in
    // Http1ClientPollOperationPriv::setError — fire the connection's
    // setClosed hook, then null the back-pointer so later callbacks
    // cannot touch a connection that may already be tearing down.
    //
    // setClosed() is thread-safe (AbstractHttpPollConnectionPriv takes
    // its own lock) so calling it from the I/O thread is fine.
    notifyOwnerClosed();
}

void NegotiatingConnectionPollOpPriv::abort(ExceptionSink* xsink) {
    int us;
    int64_t elapsed_us = (q_epoch_us(us) * 1000000LL + us) - submit_time_us;
    ASYNC_IO_TRACE("negotiate-abort target='%s:%d' priv=%p elapsed_us=%lld\n",
        target_host.c_str(), target_port, (void*)this, (long long)elapsed_us);
    qore_async_io_log(QORE_LOG_LEVEL_DEBUG,
        "negotiate-abort target='%s:%d' priv=%p elapsed_us=%lld",
        target_host.c_str(), target_port, (void*)this, (long long)elapsed_us);

    neg_state.store(NegState::CLOSED, std::memory_order_release);
    releaseCurrentOp(xsink);
    notifyOwnerClosed();
}

void NegotiatingConnectionPollOpPriv::notifyOwnerReady(std::string&& alpn) {
    AutoLocker al(owner_lock);
    if (owner_conn) {
        owner_conn->onAlpnDecided(std::move(alpn));
    }
}

void NegotiatingConnectionPollOpPriv::notifyOwnerClosed() {
    AutoLocker al(owner_lock);
    if (owner_conn) {
        owner_conn->setClosed();
        owner_conn = nullptr;
    }
}

// ============================================================
// NegotiatingHttpClientConnection
// ============================================================

NegotiatingHttpClientConnection::NegotiatingHttpClientConnection(
        const char* target_host, int target_port,
        const Http1SslConfig& ssl_config, ExceptionSink* xsink)
    : HttpClientConnectionBase(target_host, target_port,
          /* ssl_required */ true) {
    if (buildAndSubmit(ssl_config, xsink)) {
        return;
    }
}

NegotiatingHttpClientConnection::~NegotiatingHttpClientConnection() {
    ExceptionSink xsink;
    if (!taken_over) {
        closeConnection(&xsink);
    }
    if (neg_priv) {
        neg_priv->clearOwner();
        neg_priv = nullptr;
    }
    if (poll_op_obj) {
        poll_op_obj->deref(&xsink);
        poll_op_obj = nullptr;
    }
    if (sock_obj) {
        sock_obj->deref(&xsink);
        sock_obj = nullptr;
    }
    sock_priv = nullptr;
    xsink.clear();
}

int NegotiatingHttpClientConnection::buildAndSubmit(const Http1SslConfig& ssl_config,
        ExceptionSink* xsink) {
    QoreProgram* pgm = getProgram();

    // 1. Create the socket priv and its QoreObject wrapper.
    ReferenceHolder<QoreSocketObject> sock_priv_holder(new QoreSocketObject, xsink);
    QoreSocketObject* sock_priv_raw = *sock_priv_holder;

    // 2. Apply ALPN: offer h2 first (preferred), fall back to http/1.1.
    //    Must be set before the connect+SSL handshake starts.
    {
        ReferenceHolder<QoreListNode> protocols(new QoreListNode(autoTypeInfo), xsink);
        protocols->push(new QoreStringNode("h2"), xsink);
        protocols->push(new QoreStringNode("http/1.1"), xsink);
        sock_priv_raw->setAlpnProtocols(*protocols, xsink);
        if (*xsink) {
            return -1;
        }
    }

    // 3. Apply SSL configuration.
    sock_priv_raw->setSslVerifyMode(ssl_config.verify_mode);
    sock_priv_raw->acceptAllCertificates(ssl_config.accept_all);
    if (ssl_config.cert) {
        ssl_config.cert->ref();
        sock_priv_raw->setCertificate(ssl_config.cert);
    }
    if (ssl_config.key) {
        ssl_config.key->ref();
        sock_priv_raw->setPrivateKey(ssl_config.key);
    }

    ReferenceHolder<QoreObject> sock_obj_holder(
        new QoreObject(QC_SOCKET, pgm, sock_priv_holder.release()), xsink);

    // 4. Build the connect target string "host:port".
    char target_str[256];
    int n = snprintf(target_str, sizeof(target_str), "%s:%d",
        target_host.c_str(), target_port);
    if (n <= 0 || (size_t)n >= sizeof(target_str)) {
        xsink->raiseException("HTTPCLIENT-NEGOTIATE-ERROR",
            "connect target host:port string too long: host='%s' port=%d",
            target_host.c_str(), target_port);
        return -1;
    }

    // 5. Create the SocketConnectPollOperation with ssl=true.  It will
    //    drive TCP connect followed by the TLS handshake.
    sock_priv_raw->ref();
    ReferenceHolder<SocketConnectPollOperation> connect_op(
        new SocketConnectPollOperation(xsink, true, target_str, sock_priv_raw, true),
        xsink);
    if (*xsink) {
        return -1;
    }

    // 6. Create our NegotiatingConnectionPollOpPriv wrapping the connect
    //    op.  Bumps sock_priv_raw ref for the priv's own ownership.
    sock_priv_raw->ref();
    SocketConnectPollOperation* connect_ptr = connect_op.release();
    ReferenceHolder<NegotiatingConnectionPollOpPriv> priv_holder(
        new NegotiatingConnectionPollOpPriv(
            /* self */ nullptr,
            /* sock */ sock_priv_raw,
            /* connect_op */ connect_ptr,
            /* target_host */ target_host,
            /* target_port */ target_port,
            /* owner_conn */ this),
        xsink);
    NegotiatingConnectionPollOpPriv* priv_raw = *priv_holder;

    // 7. Wrap the priv in a QoreObject under QC_SOCKETPOLLOPERATIONBASE.
    //    The controller's getReferencedPrivateData(CID_SOCKETPOLLOPERATIONBASE)
    //    cast works because our priv IS-A SocketPollOperationBase.
    ReferenceHolder<QoreObject> poll_obj_holder(
        new QoreObject(QC_SOCKETPOLLOPERATIONBASE, pgm, priv_holder.release()),
        xsink);

    // 8. Wire self references.
    priv_raw->setSelf(*poll_obj_holder);
    connect_ptr->setSelf(*poll_obj_holder);

    // 9. Set the poll op's "sock" and "goal" members so SocketPollInfo
    //    hashes from inner ops have the right socket ref.
    poll_obj_holder->setValue("sock", sock_obj_holder->refSelf(), xsink);
    if (*xsink) {
        return -1;
    }
    poll_obj_holder->setValue("goal", new QoreStringNode("negotiate"), xsink);
    if (*xsink) {
        return -1;
    }

    // 10. Submit to the global AsyncIoController.
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
            xsink->raiseException("HTTPCLIENT-NEGOTIATE-ERROR",
                "failed to get AsyncIoController singleton");
        }
        return -1;
    }

    char owner_buf[64];
    snprintf(owner_buf, sizeof(owner_buf), "http-neg-%p", (void*)this);

    ReferenceHolder<QoreHashNode> info(new QoreHashNode(autoTypeInfo), xsink);
    info->setKeyValue("sock", sock_obj_holder->refSelf(), xsink);
    info->setKeyValue("spop", poll_obj_holder->refSelf(), xsink);
    info->setKeyValue("owner", new QoreStringNode(owner_buf), xsink);
    info->setKeyValue("to", -1, xsink);
    if (*xsink) {
        return -1;
    }

    // submit() takes ownership of `info` (see the ReferenceHolder in
    // AsyncIoControllerPriv::submit that releases it on every return path);
    // use .release() to transfer our ref, not *info which would double-deref.
    ReferenceHolder<QoreObject> submit_rv(
        ctl_priv_holder->submit(*ctl_obj_holder, info.release(), false, xsink), xsink);
    if (*xsink) {
        return -1;
    }

    // Commit ownership to members.
    //
    // Ordering note: the commits happen AFTER submit() so that on submit
    // failure (xsink set) the holders' ReferenceHolder dtors release the
    // refs cleanly and we never leave members pointing at a half-submitted
    // op.  An I/O thread that picks up the SubmitOp can race ahead and
    // call onAlpnDecided -> onConnectionReady before we reach these lines,
    // but that path only touches the priv's owner_conn (a raw pointer set
    // in the priv ctor before submit) and the base class condition
    // variable — it does NOT read sock_priv / submitted_to_controller.
    // closeConnection (which DOES read those) is only invoked after
    // buildAndSubmit returns (from the manager's error path or this
    // object's destructor), by which time the commits below have run.
    sock_priv = sock_priv_raw;
    sock_obj = sock_obj_holder.release();
    neg_priv = priv_raw;
    poll_op_obj = poll_obj_holder.release();
    submitted_to_controller = true;
    return 0;
}

void NegotiatingHttpClientConnection::onAlpnDecided(std::string alpn) {
    // Called on the I/O thread from NegotiatingConnectionPollOpPriv
    // after a successful connect+SSL handshake.  Store the ALPN id and
    // broadcast the READY state via the base class condition variable.
    alpn_result = std::move(alpn);
    onConnectionReady();
}

HttpClientConnectionBase* NegotiatingHttpClientConnection::takeOver(
        int max_concurrent_streams, HttpClientConnectionManagerBase* mgr,
        ExceptionSink* xsink) {
    if (taken_over) {
        xsink->raiseException("HTTPCLIENT-NEGOTIATE-ERROR",
            "negotiating connection has already been taken over");
        return nullptr;
    }
    if (!isReady()) {
        xsink->raiseException("HTTPCLIENT-NEGOTIATE-ERROR",
            "negotiating connection is not ready");
        return nullptr;
    }
    if (!sock_obj || !sock_priv) {
        xsink->raiseException("HTTPCLIENT-NEGOTIATE-ERROR",
            "negotiating connection has no socket to hand off");
        return nullptr;
    }

    // The I/O controller has finalized the neg poll op (DECIDED →
    // goalReached returns true, needsCloseOnComplete returns false).
    // The inner SocketConnectPollOperation's dtor has cleared
    // non_block_flags when its refcount dropped, so sock_priv is in an
    // Unclaimed state and can be re-submitted by the adopt-socket ctor.

    // Transfer the socket QoreObject wrapper (and the priv it owns)
    // atomically to the concrete H1/H2 connection.  Do NOT deref the
    // wrapper here — QoreSocketObject::deref closes the fd on refcount 0
    // (priv->socket->cleanup → close_internal), and the QoreObject ref
    // count IS at 1 by this point (the controller's Phase 3 cleanup has
    // already dropped pinfo.sock_obj), so a deref would immediately kill
    // the adopted socket.  The adopt-socket constructor takes ownership
    // of the wrapper directly: the H1/H2 connection's sock_obj member
    // becomes the single strong-ref holder, and when it's later
    // re-submitted to the controller, pinfo.sock_obj bumps the refcount
    // back up.
    //
    // This is the exact mechanism of the Phase 5 NEGOTIATE wire-up race:
    // an earlier version of this function created a fresh QoreObject
    // wrapper in the adopt ctor and let the NEG wrapper drop to zero,
    // producing "socket closed during poll operation" on the first
    // multiplex continuePoll.  See the Phase 5 commit message.
    QoreObject* transferred_sock_obj = sock_obj;
    QoreSocketObject* transferred_sock_priv = sock_priv;
    sock_obj = nullptr;
    sock_priv = nullptr;

    // Mark as taken over so the destructor neither double-closes nor
    // double-derefs the transferred wrapper.
    taken_over = true;

    int nominal_protocol;  // 1 = H1, 2 = H2
    if (alpn_result == "h2") {
        nominal_protocol = 2;
    } else {
        nominal_protocol = 1;  // http/1.1 or empty → H1
    }

    HttpClientConnectionBase* concrete = nullptr;
    if (nominal_protocol == 2) {
        concrete = new Http2ClientConnection(
            /* adopted_sock_obj */ transferred_sock_obj,
            /* adopted_sock_priv */ transferred_sock_priv,
            /* target_host */ target_host,
            /* target_port */ target_port,
            /* max_concurrent_streams */ max_concurrent_streams,
            xsink, mgr);
    } else {
        concrete = new Http1ClientConnection(
            /* adopted_sock_obj */ transferred_sock_obj,
            /* adopted_sock_priv */ transferred_sock_priv,
            /* target_host */ target_host,
            /* target_port */ target_port,
            /* ssl_required */ true,
            xsink, mgr);
    }

    if (*xsink) {
        if (concrete) {
            concrete->deref(xsink);
        }
        return nullptr;
    }

    return concrete;
}

void NegotiatingHttpClientConnection::closeConnection(ExceptionSink* xsink) {
    markInvalidated();
    drainInFlight();

    if (neg_priv) {
        neg_priv->clearOwner();
    }

    if (taken_over) {
        setClosed();
        return;
    }

    // Cancel from the AsyncIoController.  doCancelIntern will abort our
    // neg poll op which transitions it to CLOSED and clears the inner
    // connect op (which in turn clears non_block_flags).  Always cancel
    // when submitted, even if the base connection state is already CLOSED:
    // the I/O controller can still hold the poll op until the current
    // callback is finalized.
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
    } else if (!isClosed() && neg_priv) {
        neg_priv->abort(xsink);
    }

    setClosed();
}

QoreHashNode* NegotiatingHttpClientConnection::getReferencedErrorInfo() {
    if (!neg_priv) {
        return nullptr;
    }
    return neg_priv->getReferencedErrorInfo();
}

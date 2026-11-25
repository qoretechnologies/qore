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
#include <qore/QoreFuture.h>
#include <qore/AsyncCompletionAction.h>
#include <qore/HttpClientConnectionManager.h>
#include "qore/intern/QC_SSLTrustStore.h"
#include "qore/intern/QoreChannel.h"
#include "qore/intern/QC_Future.h"
#include "qore/intern/QC_FutureImpl.h"
#include "qore/intern/QoreHttp3ClientConnection.h"
#include "qore/intern/QC_Http3ClientPollOperationBase.h"
#include "qore/intern/QC_SocketPollOperation.h"
#include "qore/intern/QC_Socket.h"
#include "qore/intern/AsyncIoControllerPriv.h"
#include "qore/intern/QuicSession.h"
#include "qore/intern/QoreLibIntern.h"
#include "qore/intern/ql_debug.h"
#include "qore/intern/QoreSocketObjectPrivate.h"
#include "qore/intern/qore_socket_private.h"

#include <cstdio>

#include <sys/socket.h>
#include <netdb.h>

// QPP-generated symbols — forward-declared for the single-compilation-unit
extern QoreClass* QC_HTTP3CLIENTPOLLOPERATIONBASE;
extern qore_classid_t CID_HTTP3CLIENTPOLLOPERATIONBASE;

Http3ClientConnection::Http3ClientConnection(const char* target_host, int target_port,
        int max_concurrent_streams, ExceptionSink* xsink,
        HttpClientConnectionManagerBase* mgr,
        int ssl_verify_mode,
        bool ssl_accept_all_certs,
        QoreSSLCertificate* client_cert,
        QoreSSLPrivateKey* client_key,
        const std::string& ssl_ca_file,
        const std::string& ssl_ca_path,
        QoreSSLTrustStore* ssl_trust_store)
    : HttpClientConnectionBase(target_host, target_port, /* ssl_required (always for H3) */ true),
      max_concurrent_streams_(max_concurrent_streams),
      ssl_verify_mode_(ssl_verify_mode),
      ssl_accept_all_certs_(ssl_accept_all_certs),
      ssl_ca_file_(ssl_ca_file),
      ssl_ca_path_(ssl_ca_path),
      ssl_trust_store_(ssl_trust_store),
      client_cert_(client_cert),
      client_key_(client_key) {
    // a reference of its own: the connection can outlive its manager
    if (ssl_trust_store_) {
        ssl_trust_store_->ref();
    }
    if (client_cert_) {
        client_cert_->ref();
    }
    if (client_key_) {
        client_key_->ref();
    }
    if (mgr) {
        setManager(mgr);
        // Snapshot the manager's connect_timeout once at construction time so
        // the inner QUIC handshake op can enforce a transport-level deadline.
        // The manager_ back-pointer is protected by onclose_lock and may be
        // nulled by setManager(nullptr) during teardown — capturing the value
        // up-front avoids racing with that nulling on the I/O thread.
        int64_t connect_timeout_ms = mgr->getOptions().connect_timeout_ms;
        if (connect_timeout_ms > 0) {
            handshake_timeout_ns_ = connect_timeout_ms * 1000000LL;
        }
    }
    if (buildAndSubmit(xsink)) {
        return;
    }
}

Http3ClientConnection::~Http3ClientConnection() {
    ExceptionSink xsink;
    closeConnection(&xsink);
    if (ssl_trust_store_) {
        ssl_trust_store_->deref();
    }
    // the members are written under op_mu_, as readers that are not gated by the connection's state take the
    // poll operation under it; the objects are released after the lock
    QoreObject* poll_op_obj_to_deref;
    QoreObject* sock_obj_to_deref;
    {
        std::lock_guard<std::mutex> lk(op_mu_);
        poll_op_obj_to_deref = poll_op_obj;
        sock_obj_to_deref = sock_obj;
        poll_op_obj = nullptr;
        sock_obj = nullptr;
        poll_op_priv = nullptr;
        sock_priv = nullptr;
    }
    if (poll_op_obj_to_deref) {
        poll_op_obj_to_deref->deref(&xsink);
    }
    if (sock_obj_to_deref) {
        sock_obj_to_deref->deref(&xsink);
    }
    if (client_cert_) {
        client_cert_->deref();
        client_cert_ = nullptr;
    }
    if (client_key_) {
        client_key_->deref();
        client_key_ = nullptr;
    }
    xsink.clear();
}

void Http3ClientConnection::setMaxResponseBodySizeHook(int64_t max_size) {
    MethodGuard g(this);
    if (!g.acquired()) {
        return;
    }
    std::lock_guard<std::mutex> lk(op_mu_);
    if (sock_priv) {
        qore_socket_object_private(sock_priv).setMaxResponseBodySize(max_size);
    }
}

int Http3ClientConnection::buildAndSubmit(ExceptionSink* xsink) {
    QoreProgram* pgm = getProgram();

    // 1. An unbound UDP socket: the poll operation resolves the host name in all families and races connection
    //    attempts to its addresses (RFC 8305), each with a UDP socket of its own, bound on the I/O thread with the
    //    network policy check of a bind; the socket takes the descriptor of the attempt that wins.  Nothing that can
    //    block runs here.
    ReferenceHolder<QoreSocketObject> sock_priv_holder(new QoreSocketObject, xsink);
    QoreSocketObject* sock_priv_raw = *sock_priv_holder;

    // 1a. Propagate HTTPClient's SSL config onto the socket — QuicSession::createClient reads ssl_verify_mode /
    //     accept_all_certs / cert / pk / the trust configuration from it for the TLS 1.3 handshake that is
    //     integrated into QUIC, for each connection attempt.  Without this, H3 auto-upgrade always handshakes with
    //     SSL_VERIFY_NONE, breaking mTLS (clientCertTest) and other verify-mode paths.
    if (ssl_verify_mode_) {
        sock_priv_raw->setSslVerifyMode(ssl_verify_mode_);
    }
    if (ssl_accept_all_certs_) {
        sock_priv_raw->acceptAllCertificates(true);
    }
    if (ssl_trust_store_) {
        // an in-memory trust store, which takes precedence over the CA locations (qoretechnologies/qore#7248)
        qore_socket_object_private(sock_priv_raw).setSslTrustStoreForAsyncPoll(ssl_trust_store_, xsink);
        if (*xsink) {
            return -1;
        }
    }
    if (!ssl_ca_file_.empty() || !ssl_ca_path_.empty()) {
        // the socket is not submitted yet, so it is configured directly
        qore_socket_object_private(sock_priv_raw).setSslCaLocationsForAsyncPoll(ssl_ca_file_.empty()
            ? nullptr : ssl_ca_file_.c_str(), ssl_ca_path_.empty() ? nullptr : ssl_ca_path_.c_str(), xsink);
        if (*xsink) {
            return -1;
        }
    }
    if (client_cert_ && client_key_) {
        client_cert_->ref();
        client_key_->ref();
        sock_priv_raw->setCertificateAndPrivateKey(client_cert_, client_key_);
    } else {
        if (client_cert_) {
            client_cert_->ref();
            sock_priv_raw->setCertificate(client_cert_);
        }
        if (client_key_) {
            client_key_->ref();
            sock_priv_raw->setPrivateKey(client_key_);
        }
    }

    ReferenceHolder<QoreObject> sock_obj_holder(new QoreObject(QC_SOCKET, pgm, sock_priv_holder.release()), xsink);

    // 2. The SocketQuicClientPollOperation (QUIC handshake inner op), which binds the unbound socket.
    sock_priv_raw->ref();
    ReferenceHolder<SocketQuicClientPollOperation> inner_op(new SocketQuicClientPollOperation(xsink, sock_priv_raw,
        target_host.c_str(), static_cast<uint16_t>(target_port), AF_UNSPEC, handshake_timeout_ns_,
        /* bind_unbound */ true), xsink);
    if (*xsink) {
        return -1;
    }

    // 3. The Http3ClientPollOperationPriv.
    sock_priv_raw->ref();
    SocketQuicClientPollOperation* inner_ptr = inner_op.release();
    ReferenceHolder<Http3ClientPollOperationPriv> priv_holder(new Http3ClientPollOperationPriv(/* self */ nullptr,
        /* sock */ sock_priv_raw, /* inner */ inner_ptr, /* conn_priv */ this), xsink);
    Http3ClientPollOperationPriv* priv_raw = *priv_holder;
    // the connect timeout also bounds the wait of an extended CONNECT for the server's SETTINGS
    priv_raw->setConnectTimeout(handshake_timeout_ns_ / 1000000LL);

    // 4. Wrap in QoreObject.
    ReferenceHolder<QoreObject> poll_obj_holder(
        new QoreObject(QC_HTTP3CLIENTPOLLOPERATIONBASE, pgm, priv_holder.release()), xsink);

    // 5. Set self references.
    priv_raw->setSelf(*poll_obj_holder);
    inner_ptr->setSelf(*poll_obj_holder);

    // 6. Set QoreObject members.
    qore_poll_op_set_member(poll_obj_holder, "sock", sock_obj_holder->refSelf(), xsink);
    if (*xsink) {
        return -1;
    }
    qore_poll_op_set_member(poll_obj_holder, "goal", new QoreStringNode("http3_client"), xsink);
    if (*xsink) {
        return -1;
    }

    // 7. Publish the connection's members before the submission, after which the I/O thread can report the outcome;
    //    from now on the destructor releases the objects, and closeConnection() aborts an operation not submitted
    ReferenceHolder<QoreObject> submit_sock(sock_obj_holder->objectRefSelf(), xsink);
    ReferenceHolder<QoreObject> submit_op(poll_obj_holder->objectRefSelf(), xsink);
    {
        std::lock_guard<std::mutex> lk(op_mu_);
        qore_socket_object_private(sock_priv_raw).setMaxResponseBodySize(max_response_body_size_);
        sock_priv = sock_priv_raw;
        sock_obj = sock_obj_holder.release();
        poll_op_priv = priv_raw;
        poll_op_obj = poll_obj_holder.release();
    }

    // 8. Submit to the AsyncIoController.
    ReferenceHolder<QoreObject> ctl_obj_holder(qore_get_async_io_controller_obj(xsink), xsink);
    if (*xsink || !ctl_obj_holder) {
        return -1;
    }
    ReferenceHolder<AsyncIoControllerPriv> ctl_priv_holder(static_cast<AsyncIoControllerPriv*>(
        ctl_obj_holder->getReferencedPrivateData(CID_ASYNCIOCONTROLLER, xsink)), xsink);
    if (*xsink || !ctl_priv_holder) {
        if (!*xsink) {
            xsink->raiseException("HTTPCLIENT-CONNECT-ERROR", "failed to get AsyncIoController singleton");
        }
        return -1;
    }
    char owner_buf[64];
    if (owner_str.empty()) {
        snprintf(owner_buf, sizeof(owner_buf), "http3-cpp-conn-%p", static_cast<const void*>(this));
    }
    ReferenceHolder<QoreHashNode> info(new QoreHashNode(autoTypeInfo), xsink);
    info->setKeyValue("sock", submit_sock.release(), xsink);
    info->setKeyValue("spop", submit_op.release(), xsink);
    info->setKeyValue("owner", new QoreStringNode(owner_str.empty() ? owner_buf : owner_str.c_str()), xsink);
    // Disable controller-level timeout; the H3 poll op manages its own idle timeout.
    info->setKeyValue("to", -1, xsink);
    if (*xsink) {
        return -1;
    }
    // submit() takes ownership of `info`
    ReferenceHolder<QoreObject> submit_rv(ctl_priv_holder->submit(*ctl_obj_holder, info.release(), false, xsink),
        xsink);
    if (*xsink) {
        return -1;
    }
    std::lock_guard<std::mutex> lk(op_mu_);
    submitted_to_controller = true;
    return 0;
}

int Http3ClientConnection::getActiveStreamCount() const {
    // MethodGuard: block concurrent closeConnection from tearing down
    // poll_op_priv while we read it.  If the connection has already
    // been invalidated, report 0 (same as the existing !poll_op_priv
    // branch — this is a count accessor, not a state transition).
    MethodGuard g(const_cast<Http3ClientConnection*>(this));
    if (!g.acquired()) {
        return 0;
    }
    // the poll operation is taken with a reference of its own under op_mu_, which guards the members
    Http3PollOperationReaderRef op(getReferencedPollOp());
    return op ? op->getActiveStreamCount() : 0;
}

Http3ClientPollOperationPriv* Http3ClientConnection::getReferencedPollOp() const {
    std::lock_guard<std::mutex> lk(op_mu_);
    if (poll_op_priv) {
        poll_op_priv->ref();
    }
    return poll_op_priv;
}

QoreHashNode* Http3ClientConnection::submitRequest(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        ExceptionSink* xsink, HttpClientEventSink* event_sink) {
    MethodGuard g(this);
    // the poll operation is taken with a reference of its own under op_mu_, which guards the members
    Http3PollOperationReaderRef op_ref(getReferencedPollOp());
    if (!g.acquired()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit request: connection has been closed");
        return nullptr;
    }
    if (!op_ref || isClosed()) {
        raiseClosedSubmitError("cannot submit request: connection is closed",
            xsink);
        return nullptr;
    }
    if (!isReady()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit request: connection is not ready");
        return nullptr;
    }

    // Return {future, stream_id} shape to match H1/H2 so
    // HttpClientConnectionManagerBase::request can await the response
    // uniformly via q_future_get_blocking.  The earlier variant blocked
    // on the Future internally and returned the response hash directly,
    // which mgr.request rejected with "submitRequest result missing
    // 'future' key".
    ReferenceHolder<QorePromise> promise_holder(new QorePromise(), xsink);
    QorePromise* promise_raw = *promise_holder;
    ReferenceHolder<QoreFuture> future_holder(promise_holder->getFuture(xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    QoreProgram* pgm = getProgram();
    ReferenceHolder<QoreObject> future_obj(
        qore_new_future_impl_object(pgm, future_holder.release()), xsink);

    PromiseAction* action = new PromiseAction(promise_raw, /* promise_obj */ nullptr);
    // the events of this request are reported to the sink of the client that issued it
    action->setEventSink(event_sink);

    int64_t stream_id = op_ref->submitRequest(method, path, headers,
        body, body_len, /* streaming */ false, action,
        max_concurrent_streams_, xsink);
    if (*xsink || stream_id < 0) {
        return nullptr;
    }

    releaseStreamReservation();

    // Wake the I/O controller so QUIC packets are produced and sent on the I/O thread.
    wakeController();

    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);
    result->setKeyValue("stream_id", QoreValue((int64)stream_id), xsink);
    result->setKeyValue("future", future_obj.release(), xsink);
    promise_holder.release()->deref(xsink);
    return result.release();
}

int64_t Http3ClientConnection::submitRequestWithAction(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        AbstractAsyncAction* action, ExceptionSink* xsink) {
    MethodGuard g(this);
    // the poll operation is taken with a reference of its own under op_mu_, which guards the members
    Http3PollOperationReaderRef op_ref(getReferencedPollOp());
    if (!g.acquired()) {
        action->deref(xsink);
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit request: connection has been closed");
        return -1;
    }
    if (!op_ref || isClosed()) {
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

    int64_t stream_id = op_ref->submitRequest(method, path, headers,
        body, body_len, /* streaming */ false, action,
        max_concurrent_streams_, xsink);
    if (*xsink || stream_id < 0) {
        return -1;
    }

    releaseStreamReservation();

    // Wake the I/O controller so QUIC packets are produced and sent on the I/O thread.
    wakeController();

    return stream_id;
}

int64_t Http3ClientConnection::submitRequestStreaming(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        QoreChannel*& channel_out, ExceptionSink* xsink, HttpClientEventSink* event_sink) {
    MethodGuard g(this);
    // the poll operation is taken with a reference of its own under op_mu_, which guards the members
    Http3PollOperationReaderRef op_ref(getReferencedPollOp());
    if (!g.acquired()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit streaming request: connection has been closed");
        return -1;
    }
    if (!op_ref || isClosed()) {
        raiseClosedSubmitError(
            "cannot submit streaming request: connection is closed", xsink);
        return -1;
    }
    if (!isReady()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit streaming request: connection is not ready");
        return -1;
    }

    // Create Channel for incremental response delivery
    ReferenceHolder<QoreChannel> ch_holder(new QoreChannel(-1), xsink);
    QoreChannel* ch = *ch_holder;

    ChannelAction* action = new ChannelAction(ch);
    action->setEventSink(event_sink);

    // streaming=false: `streaming` at this layer means request-body streaming
    // (caller pushes body via sendStreamData).  submitRequestStreaming uses a
    // one-shot body and only streams the response, so HEADERS must carry
    // END_STREAM (via nghttp3 with drp=nullptr) when the body is empty —
    // otherwise the server waits for body data that never arrives.
    int64_t stream_id = op_ref->submitRequest(method, path, headers,
        body, body_len, /* streaming */ false, action,
        max_concurrent_streams_, xsink);
    if (*xsink || stream_id < 0) {
        return -1;
    }

    releaseStreamReservation();

    // Wake the I/O controller so QUIC packets are produced and sent on the I/O thread.
    wakeController();

    ch->ref();
    channel_out = ch;
    return stream_id;
}

QoreHashNode* Http3ClientConnection::submitRequestStreamingSend(const char* method,
        const char* path, const QoreHashNode* headers, bool streaming_recv,
        QoreChannel*& channel_out, ExceptionSink* xsink, HttpClientEventSink* event_sink) {
    MethodGuard g(this);
    // the poll operation is taken with a reference of its own under op_mu_, which guards the members
    Http3PollOperationReaderRef op_ref(getReferencedPollOp());
    if (!g.acquired()) {
        releaseStreamReservation(true);
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit streaming send request: connection has been closed");
        return nullptr;
    }
    if (!op_ref || isClosed()) {
        releaseStreamReservation(true);
        raiseClosedSubmitError(
            "cannot submit streaming send request: connection is closed",
            xsink);
        return nullptr;
    }
    if (!isReady()) {
        releaseStreamReservation(true);
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot submit streaming send request: connection is not ready");
        return nullptr;
    }
    if (!beginStreamingSend()) {
        releaseStreamReservation(true);
        xsink->raiseException("HTTPCLIENT-CAPACITY-ERROR",
            "cannot submit streaming send request: another streaming send "
            "request is already in progress on this connection");
        return nullptr;
    }
    StreamingSendSetupGuard setup_guard(this);

    AbstractAsyncAction* action = nullptr;
    ReferenceHolder<QoreObject> future_obj(xsink);
    ReferenceHolder<QoreChannel> ch_holder(xsink);

    if (streaming_recv) {
        ch_holder = new QoreChannel(-1);
        action = new ChannelAction(*ch_holder);
    } else {
        ReferenceHolder<QorePromise> promise_holder(new QorePromise(), xsink);
        QorePromise* promise_raw = *promise_holder;
        ReferenceHolder<QoreFuture> future_holder(promise_holder->getFuture(xsink), xsink);
        if (*xsink) {
            releaseStreamReservation();
            return nullptr;
        }
        QoreProgram* pgm = getProgram();
        future_obj = qore_new_future_impl_object(pgm, future_holder.release());
        if (*xsink) {
            releaseStreamReservation();
            return nullptr;
        }
        action = new PromiseAction(promise_raw, nullptr);
        promise_holder.release()->deref(xsink);
    }
    action->setEventSink(event_sink);

    // Submit with streaming=true (no END_STREAM — bidirectional streaming)
    int64_t stream_id = op_ref->submitRequest(method, path, headers,
        nullptr, 0, /* streaming */ true, action,
        max_concurrent_streams_, xsink);
    if (*xsink || stream_id < 0) {
        releaseStreamReservation();
        return nullptr;
    }

    // Store stream_id for subsequent pushSendData/setTrailers calls
    streaming_send_stream_id = stream_id;
    setup_guard.markSubmitted();

    releaseStreamReservation();

    // Wake the I/O controller so QUIC packets are produced and sent on the I/O thread.
    wakeController();

    // Build result
    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);
    result->setKeyValue("stream_id", QoreValue((int64)stream_id), xsink);
    if (*xsink) {
        return nullptr;
    }
    if (streaming_recv) {
        QoreChannel* ch = *ch_holder;
        ch->ref();
        channel_out = ch;
    } else {
        result->setKeyValue("future", future_obj.release(), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    setup_guard.keepOpen();
    return result.release();
}

void Http3ClientConnection::pushSendData(const void* data, size_t len, ExceptionSink* xsink) {
    MethodGuard g(this);
    // the poll operation is taken with a reference of its own under op_mu_, which guards the members
    Http3PollOperationReaderRef op_ref(getReferencedPollOp());
    if (!g.acquired()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot push data: connection has been closed");
        return;
    }
    if (!op_ref) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot push data: connection has no poll operation");
        return;
    }
    if (streaming_send_stream_id < 0) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot push data: no streaming send in progress");
        return;
    }
    if (!op_ref->isReady()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot push data: connection is not ready");
        return;
    }

    if (!sock_priv) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot push data: QUIC session not available");
        return;
    }

    bool end_stream = (!data || !len);
    if (end_stream) {
        // End-of-body: send empty DATA with end_stream flag
        sendStreamDataWaitingForDrain(streaming_send_stream_id, nullptr, 0, true, xsink);
        streaming_send_stream_id = -1;
        finishStreamingSend();
    } else {
        sendStreamDataWaitingForDrain(streaming_send_stream_id, data, len, false, xsink);
    }
}

void Http3ClientConnection::sendStreamDataWaitingForDrain(int64_t stream_id, const void* data, size_t len,
        bool end_stream, ExceptionSink* xsink) {
    // the wait is bounded by the request timeout of stalled time, not of total time: a large body that a slow peer
    // takes makes progress all the while (as HttpClientIo's Http3ClientConnection::sendStreamData(), issue #6968)
    int stall_timeout_ms = -1;
    {
        AutoLocker al(onclose_lock);
        if (manager_ && manager_->getOptions().request_timeout_ms > 0) {
            stall_timeout_ms = manager_->getOptions().request_timeout_ms;
        }
    }
    while (true) {
        // 1: the stream's send buffer is full and took nothing of the data
        int rv = sock_priv->sendQuicClientStreamData(stream_id, data, len, end_stream, xsink);
        if (*xsink || rv != 1) {
            return;
        }
        // the send waits for the buffer to drain and is made again (qore#7465); a send on the I/O thread cannot wait
        if (qore_on_async_io_thread()) {
            xsink->raiseException("HTTP3-FLOW-CONTROL", "stream %lld: the send buffer is full; the data was not sent",
                (long long)stream_id);
            return;
        }
        // the staged data is sent by the I/O thread
        wakeController();
        int64_t acked_before = qore_socket_object_quic_client_stream_acked(sock_priv, stream_id, xsink);
        // 0: drained, 1: timed out, -1: the stream has no data staged any more (sent, or the stream or the
        // connection ended: a reset or the close of the connection ends the wait at once); the send is made again in
        // each case, and reports a stream or a connection that ended
#ifdef DEBUG
        qore_dbg_http_hold(QoreDbgHttpHoldPoint::SendDrainWait, this);
#endif
        int rc = *xsink ? -1 : sock_priv->waitForQuicClientStreamDrain(stream_id, stall_timeout_ms, xsink);
        if (*xsink) {
            // the wait failed: the thread was cancelled (THREAD-CANCELLED), its Program was interrupted, or the
            // session is gone; the data was not sent
            return;
        }
        if (rc == 1) {
            // no drain within the timeout: a stall only if the peer acknowledged nothing of the stream's data
            int64_t acked_after = qore_socket_object_quic_client_stream_acked(sock_priv, stream_id, xsink);
            if (*xsink) {
                return;
            }
            if (acked_after <= acked_before) {
                xsink->raiseException("HTTP3-FLOW-CONTROL", "stream %lld: the peer acknowledged none of the "
                    "stream's data for %d ms; the data was not sent", (long long)stream_id, stall_timeout_ms);
                return;
            }
        }
    }
}

void Http3ClientConnection::setTrailers(const QoreHashNode* trailers, ExceptionSink* xsink) {
    MethodGuard g(this);
    // the poll operation is taken with a reference of its own under op_mu_, which guards the members
    Http3PollOperationReaderRef op_ref(getReferencedPollOp());
    if (!g.acquired()) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot set trailers: connection has been closed");
        return;
    }
    if (!op_ref) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot set trailers: connection has no poll operation");
        return;
    }
    if (streaming_send_stream_id < 0) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot set trailers: no streaming send in progress");
        return;
    }

    if (!sock_priv) {
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot set trailers: QUIC session not available");
        return;
    }

    qore_socket_object_private(sock_priv).submitQuicClientTrailers(streaming_send_stream_id, trailers, xsink);
}

bool Http3ClientConnection::cancelRequest(int64_t stream_id, ExceptionSink* xsink) {
    MethodGuard g(this);
    // the poll operation is taken with a reference of its own under op_mu_, which guards the members
    Http3PollOperationReaderRef op_ref(getReferencedPollOp());
    if (!g.acquired() || !op_ref) {
        // Already closing: the poll op settles every pending stream on the
        // way out, so there is nothing left to abandon here.
        return false;
    }
    // Settle the request's completion action first.  A stream that already
    // completed reports false and is left alone — the connection stays in
    // the pool untouched.
    if (!op_ref->cancelStream(stream_id, xsink)) {
        return false;
    }
    // Then cancel the stream on the wire.  HTTP/3 multiplexes over QUIC, so
    // only this stream ends: concurrent streams on the same connection are
    // unaffected and the connection stays poolable.
    int64_t session_id = op_ref->getSessionId();
    if (session_id && sock_priv) {
        sock_priv->cancelQuicStream(session_id, stream_id, xsink);
    }
    return true;
}

void Http3ClientConnection::closeConnection(ExceptionSink* xsink) {
    // Lifetime barrier: atomically mark the connection invalidated (so
    // any NEW method calls arriving after this point are refused via
    // MethodGuard.acquired()==false and return a clean Qore exception)
    // and wait for any CURRENTLY in-flight method calls to finish.  See
    // HttpClientConnection.h for the full design rationale.
    //
    // After drainInFlight() returns, no call is dereferencing the
    // protocol-specific members below and none can start, so the
    // teardown that follows is race-free with Qore-visible API callers.
    markInvalidated();
    drainInFlight();
    finishStreamingSend();

    // the poll operation and socket, read under the lock; the close is no reader: it runs outside the connection's
    // methods (it waits for them above), so the release of the operation, which can run its cleanup, is made here
    ReferenceHolder<Http3ClientPollOperationPriv> op(xsink);
    QoreSocketObject* sock = nullptr;
    bool submitted;
    {
        std::lock_guard<std::mutex> lk(op_mu_);
        if (poll_op_priv) {
            poll_op_priv->ref();
            op = poll_op_priv;
        }
        sock = sock_priv;
        submitted = submitted_to_controller;
        submitted_to_controller = false;
    }

    if (!op) {
        // the poll operation was never created — transition the base-class state to CLOSED so waiters see the
        // error
        setClosed();
        return;
    }

    // Disarm the raw connection_priv back-pointer BEFORE cancel — same
    // pattern as Http1/Http2ClientConnection::closeConnection.
    op->disarmConnectionPriv();

    // Cancel and close the op in the global AsyncIoController — this
    // synchronously waits until the I/O thread stops processing the
    // operation, then closes the socket on the controller thread.  The I/O
    // thread's cancel processing calls abort() on the poll op via
    // doCancelIntern → callAbort, so we must NOT call abort() again here.
    //
    // NOTE: we must always cancel even if isClosed() is true.  The I/O
    // thread's setError() calls connection_priv->setClosed() (which sets
    // the state to CLOSED) before nulling connection_priv.  If we skip
    // the cancel based on isClosed(), the poll op remains in the I/O
    // controller's cache with a stale connection_priv pointer — the I/O
    // thread would access freed memory when it later processes the entry.
    if (submitted && sock) {
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
                ctl_priv_holder->cancelAndClose(sock, &cancel_xsink);
            }
        }
        cancel_xsink.clear();
    } else if (!isClosed()) {
        // Not submitted to the I/O controller — abort directly.
        op->abort(xsink);
    }

    setClosed();
}

void Http3ClientConnection::wakeController() const {
    if (!sock_obj) {
        return;
    }

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

QoreHashNode* Http3ClientConnection::getReferencedErrorInfo() {
    // the poll operation is taken with a reference of its own under op_mu_, as this read is not gated by the
    // connection's state; the error of a connection that failed is the error of its connection attempt that got
    // furthest (see SocketQuicClientPollOperation)
    Http3PollOperationReaderRef op;
    {
        std::lock_guard<std::mutex> lk(op_mu_);
        if (!poll_op_priv) {
            return nullptr;
        }
        poll_op_priv->ref();
        op = poll_op_priv;
    }
#ifdef DEBUG
    // a test holds the thread here, with the operation taken, while another thread closes the connection
    qore_dbg_http_hold(QoreDbgHttpHoldPoint::TeardownRead, this);
#endif
    return op->getErrorInfo();
}

/*
    QoreSocketObject.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    provides a thread-safe interface to the QoreSocket object

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
#include <qore/InputStream.h>
#include <qore/SocketPollOperation.h>
#include "qore/intern/AsyncIoControllerPriv.h"
#include "qore/intern/qore_socket_private.h"
#include "qore/intern/QC_Socket.h"
#include "qore/intern/QC_SocketPollOperation.h"
#include "qore/intern/QC_SSLCertificate.h"
#include "qore/intern/QC_SSLPrivateKey.h"
#include "qore/intern/Http2Session.h"
#include "qore/intern/SocketSyncPoll.h"
#include "qore/intern/FileInputStream.h"
#include "qore/intern/FileOutputStream.h"

#include <atomic>
#include <limits>

static std::atomic<uint64_t> qore_socket_object_sync_exec_seq{0};

static void qore_socket_object_raise_poll_result_exception(const QoreHashNode* ex, ExceptionSink* xsink) {
    QoreValue err = ex->getKeyValue("err");
    QoreValue desc = ex->getKeyValue("desc");
    QoreValue arg = ex->getKeyValue("arg");
    xsink->raiseException(
        err.getType() == NT_STRING
            ? err.get<const QoreStringNode>()->stringRefSelf()
            : new QoreStringNode("ASYNC-IO-ERROR"),
        desc.getType() == NT_STRING
            ? desc.get<const QoreStringNode>()->stringRefSelf()
            : new QoreStringNode("async socket operation failed"),
        arg.refSelf());
}

static bool qore_socket_object_exec_exception_is(const QoreHashNode& ex, const char* err) {
    QoreValue ex_err = ex.getKeyValue("err");
    return ex_err.getType() == NT_STRING && !strcmp(ex_err.get<const QoreStringNode>()->c_str(), err);
}

static QoreHashNode* qore_socket_object_exec_poll_operation(QoreSocketObject* s, QoreObject* sock_obj,
        QoreObject* op_obj, int timeout_ms, const char* owner_name, ExceptionSink* xsink,
        QoreHashNode** ex_out = nullptr) {
    SocketSyncPoll::assertNotOnIoThread("Socket", owner_name, xsink);
    if (*xsink) {
        return nullptr;
    }

    ReferenceHolder<QoreObject> ctl_obj(qore_get_async_io_controller_obj(xsink), xsink);
    if (!ctl_obj) {
        return nullptr;
    }
    ReferenceHolder<AsyncIoControllerPriv> ctrl(
        static_cast<AsyncIoControllerPriv*>(
            (*ctl_obj)->getReferencedPrivateData(CID_ASYNCIOCONTROLLER, xsink)), xsink);
    if (*xsink || !ctrl) {
        return nullptr;
    }

    std::string owner("QoreSocketObject::sync:");
    owner += owner_name;
    owner += ':';
    owner += s->getUniqueHash();

    std::string key("QoreSocketObject::sync:");
    key += owner_name;
    key += ':';
    key += s->getUniqueHash();
    // Several synchronous callers can delegate work for the same socket at the
    // same time; keep the cache key unique while thread_key preserves affinity.
    key += ':';
    key += std::to_string(++qore_socket_object_sync_exec_seq);

    ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollOperationInfo, xsink), xsink);
    info->setKeyValue("sock", sock_obj->objectRefSelf(), xsink);
    info->setKeyValue("spop", op_obj->objectRefSelf(), xsink);
    info->setKeyValue("owner", new QoreStringNode(owner), xsink);
    info->setKeyValue("key", new QoreStringNode(key), xsink);
    info->setKeyValue("thread_key", new QoreStringNode(s->getUniqueHash()), xsink);
    info->setKeyValue("to", timeout_ms, xsink);
    if (*xsink) {
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> result(ctrl->exec(*ctl_obj, info.release(), false, xsink), xsink);
    if (*xsink || !result) {
        return nullptr;
    }

    QoreValue ex = result->getKeyValue("ex");
    if (ex.getType() == NT_HASH) {
        if (ex_out) {
            *ex_out = ex.get<const QoreHashNode>()->hashRefSelf();
            return nullptr;
        }
        qore_socket_object_raise_poll_result_exception(ex.get<const QoreHashNode>(), xsink);
        return nullptr;
    }
    return result.release();
}

class QoreSocketObjectReadinessPollOperation : public SocketPollOperationBase {
public:
    QoreSocketObjectReadinessPollOperation(ExceptionSink* xsink, QoreSocketObject* sock, unsigned direction,
            int events, const char* waiting_state, const char* ready_state)
            : sock(sock), direction(direction), events(events), waiting_state(waiting_state), ready_state(ready_state) {
        init(xsink, true);
    }

    DLLLOCAL void init(ExceptionSink* xsink, bool defer_init) {
        if (defer_init) {
            controller_deferred_tid = q_gettid();
            return;
        }

        my_socket_priv* priv = my_socket_priv::getPriv(*sock);
        AutoLocker al(priv->m);
        initLocked(xsink);
    }

    DLLLOCAL int initLocked(ExceptionSink* xsink) {
        my_socket_priv* priv = my_socket_priv::getPriv(*sock);
        assert(priv->m.trylock());
        if (initialized) {
            return 0;
        }
        if (priv->checkOpen(xsink)
                || priv->setNonBlockFromAsyncController(xsink, direction, controller_deferred_tid)) {
            return -1;
        }
        set_non_block = true;
        initialized = true;
        return 0;
    }

    DLLLOCAL virtual void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            clearNonBlock();
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return ready;
    }

    DLLLOCAL virtual void abort(ExceptionSink* xsink) override {
        clearNonBlock();
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (!initialized) {
            my_socket_priv* priv = my_socket_priv::getPriv(*sock);
            AutoLocker al(priv->m);
            if (initLocked(xsink)) {
                return nullptr;
            }
        }

        if (!waiting) {
            waiting = true;
            return getSocketPollInfoHash(xsink, events);
        }

        my_socket_priv* priv = my_socket_priv::getPriv(*sock);
        AutoLocker al(priv->m);
        if (set_non_block) {
            priv->clearNonBlock(direction);
            set_non_block = false;
        }
        if (priv->checkOpen(xsink)) {
            return nullptr;
        }
        ready = true;
        return nullptr;
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return ready;
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        return ready ? ready_state : waiting_state;
    }

private:
    DLLLOCAL void clearNonBlock() {
        if (set_non_block) {
            my_socket_priv* priv = my_socket_priv::getPriv(*sock);
            AutoLocker al(priv->m);
            priv->clearNonBlock(direction);
            set_non_block = false;
        }
    }

    QoreSocketObject* sock;
    unsigned direction;
    int events;
    const char* waiting_state;
    const char* ready_state;
    bool waiting = false;
    bool ready = false;
    bool set_non_block = false;
    bool initialized = false;
    int controller_deferred_tid = -1;
};

class QoreSocketObjectHttp1AllowedPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketObjectHttp1AllowedPollOperation(QoreSocketObject* sock, const char* mname)
            : sock(sock), mname(mname) {
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink*) override {
        done = true;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (done) {
            return nullptr;
        }

        my_socket_priv* priv = my_socket_priv::getPriv(*sock);
        if (priv->hasH2SessionForAsyncPoll()) {
            xsink->raiseException("HTTP2-ERROR",
                "HTTP/1 message attempted on HTTP/2 connection (Socket::%s)", mname.c_str());
        }
        done = true;
        return nullptr;
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        return done ? "http1-checked" : "checking-http1";
    }

private:
    QoreSocketObject* sock;
    std::string mname;
    bool done = false;
};

class QoreSocketObjectHttp2ServerStreamPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketObjectHttp2ServerStreamPollOperation(QoreSocketObject* sock, int32_t stream_id,
            const char* mname)
            : sock(sock), stream_id(stream_id), mname(mname) {
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink*) override {
        output = -1;
        done = true;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (done) {
            return nullptr;
        }

        my_socket_priv* priv = my_socket_priv::getPriv(*sock);
        if (!priv->hasH2SessionForAsyncPoll()) {
            output = -1;
        } else if (priv->isH2ServerSessionForAsyncPoll() && stream_id > 0) {
            output = stream_id;
        } else {
            xsink->raiseException("HTTP2-ERROR",
                "HTTP/1 message attempted on HTTP/2 connection (Socket::%s)", mname.c_str());
            output = -1;
        }
        done = true;
        return nullptr;
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return static_cast<int64>(output);
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        return done ? "http2-server-stream-checked" : "checking-http2-server-stream";
    }

private:
    QoreSocketObject* sock;
    int32_t stream_id;
    std::string mname;
    int32_t output = -1;
    bool done = false;
};

class QoreSocketObjectIdleDataPollOperation : public SocketPollOperationBase {
public:
    QoreSocketObjectIdleDataPollOperation(ExceptionSink* xsink, QoreSocketObject* sock) : sock(sock) {
        init(xsink, true);
    }

    DLLLOCAL void init(ExceptionSink* xsink, bool defer_init) {
        if (defer_init) {
            controller_deferred_tid = q_gettid();
            return;
        }

        my_socket_priv* priv = my_socket_priv::getPriv(*sock);
        AutoLocker al(priv->m);
        initLocked(xsink);
    }

    DLLLOCAL int initLocked(ExceptionSink* xsink) {
        my_socket_priv* priv = my_socket_priv::getPriv(*sock);
        assert(priv->m.trylock());
        if (initialized) {
            return 0;
        }
        if (priv->setNonBlockFromAsyncController(xsink, NB_RECV, controller_deferred_tid)) {
            return -1;
        }
        set_non_block = true;
        initialized = true;
        return 0;
    }

    DLLLOCAL virtual void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            clearNonBlock();
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink*) override {
        done = true;
        result = -1;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (done) {
            return nullptr;
        }

        if (!initialized) {
            my_socket_priv* priv = my_socket_priv::getPriv(*sock);
            AutoLocker al(priv->m);
            if (initLocked(xsink)) {
                done = true;
                result = -1;
                return nullptr;
            }
        }

        result = sock->checkIdleDataForAsyncPoll(xsink);
        done = true;
        return nullptr;
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return result;
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        return done ? "idle-data-checked" : "checking-idle-data";
    }

private:
    DLLLOCAL void clearNonBlock() {
        if (set_non_block) {
            my_socket_priv* priv = my_socket_priv::getPriv(*sock);
            AutoLocker al(priv->m);
            priv->clearNonBlock(NB_RECV);
            set_non_block = false;
        }
    }

    QoreSocketObject* sock;
    int result = 0;
    bool done = false;
    bool set_non_block = false;
    bool initialized = false;
    int controller_deferred_tid = -1;
};

static std::shared_ptr<QuicSession> qore_socket_object_get_quic_session(const QoreSocketObject* s,
        int64_t session_id);
static std::shared_ptr<QuicSession> qore_socket_object_get_first_quic_session(const QoreSocketObject* s);
static Http2SessionPtr qore_socket_object_get_h2_session(const QoreSocketObject* s);

class QoreSocketObjectStreamDrainPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketObjectStreamDrainPollOperation(Http2SessionPtr h2, int64_t stream_id)
            : h2(std::move(h2)), stream_id(stream_id) {
    }

    DLLLOCAL QoreSocketObjectStreamDrainPollOperation(std::shared_ptr<QuicSession> quic, int64_t stream_id)
            : quic(std::move(quic)), stream_id(stream_id) {
    }

    DLLLOCAL QoreSocketObjectStreamDrainPollOperation(QoreSocketObject* sock, int32_t stream_id, bool http2)
            : sock(sock), stream_id(stream_id), use_h2_session(http2) {
        assert(http2);
        sock->ref();
    }

    DLLLOCAL QoreSocketObjectStreamDrainPollOperation(QoreSocketObject* sock, int64_t stream_id)
            : sock(sock), stream_id(stream_id), use_first_quic_session(true) {
        sock->ref();
    }

    DLLLOCAL QoreSocketObjectStreamDrainPollOperation(QoreSocketObject* sock, int64_t session_id,
            int64_t stream_id)
            : sock(sock), stream_id(stream_id), quic_session_id(session_id) {
        sock->ref();
    }

    DLLLOCAL ~QoreSocketObjectStreamDrainPollOperation() override {
        ExceptionSink xsink;
        cleanup(&xsink);
        if (xsink) {
            xsink.clear();
        }
        if (sock) {
            sock->deref(&xsink);
            if (xsink) {
                xsink.clear();
            }
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink* xsink) override {
        output = -1;
        done = true;
        cleanup(xsink);
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (done) {
            return nullptr;
        }

        int rc = checkDrain(xsink);
        if (rc != 1) {
            output = rc;
            done = true;
            cleanup(xsink);
            return nullptr;
        }

        if (!registered && registerWaiter(xsink)) {
            output = -1;
            done = true;
            cleanup(xsink);
            return nullptr;
        }

        rc = checkDrain(xsink);
        if (rc != 1) {
            output = rc;
            done = true;
            cleanup(xsink);
            return nullptr;
        }

        if (!wake_requested) {
            wake_requested = true;
            wakeIoThread(xsink);
            if (*xsink) {
                output = -1;
                done = true;
                cleanup(xsink);
                return nullptr;
            }
        }

        return getSocketPollInfoHash(xsink, SOCK_POLLIN);
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return output;
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        return done ? "stream-drained" : "waiting-stream-drain";
    }

private:
    DLLLOCAL bool ensureSession(ExceptionSink* xsink) {
        if (h2 || quic) {
            return true;
        }
        assert(sock);
        if (use_h2_session) {
            h2 = qore_socket_object_get_h2_session(sock);
            if (!h2) {
                xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
                return false;
            }
            return true;
        }
        quic = use_first_quic_session
            ? qore_socket_object_get_first_quic_session(sock)
            : qore_socket_object_get_quic_session(sock, quic_session_id);
        if (!quic) {
            if (use_first_quic_session) {
                xsink->raiseException("QUIC-ERROR", "no active QUIC session on this socket");
            } else {
                xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
                    (long long)quic_session_id);
            }
            return false;
        }
        return true;
    }

    DLLLOCAL int checkDrain(ExceptionSink* xsink) {
        if (!ensureSession(xsink)) {
            return -1;
        }
        return h2
            ? h2->waitForStreamDrain(static_cast<int32_t>(stream_id), 0)
            : quic->waitForStreamDrain(stream_id, 0);
    }

    DLLLOCAL int registerWaiter(ExceptionSink* xsink) {
        if (!ensureSession(xsink)) {
            return -1;
        }
        ReferenceHolder<QoreObject> obj(getReferencedSocketObject(xsink), xsink);
        if (*xsink) {
            return -1;
        }
        if (h2) {
            h2->registerStreamDrainWaiter(*obj, xsink);
        } else {
            quic->registerStreamDrainWaiter(*obj, xsink);
        }
        if (*xsink) {
            return -1;
        }
        waiter_sock_obj = obj.release();
        registered = true;
        return 0;
    }

    DLLLOCAL void cleanup(ExceptionSink* xsink) {
        if (registered && waiter_sock_obj) {
            if (h2) {
                h2->unregisterStreamDrainWaiter(waiter_sock_obj, xsink);
            } else if (quic) {
                quic->unregisterStreamDrainWaiter(waiter_sock_obj, xsink);
            }
            registered = false;
        }
        if (waiter_sock_obj) {
            waiter_sock_obj->deref(xsink);
            waiter_sock_obj = nullptr;
        }
    }

    QoreSocketObject* sock = nullptr;
    Http2SessionPtr h2;
    std::shared_ptr<QuicSession> quic;
    QoreObject* waiter_sock_obj = nullptr;
    int64_t stream_id;
    int64_t quic_session_id = 0;
    int output = -1;
    bool done = false;
    bool registered = false;
    bool wake_requested = false;
    bool use_h2_session = false;
    bool use_first_quic_session = false;
};

class QoreSocketObjectQuicStreamDataPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketObjectQuicStreamDataPollOperation(QoreSocketObject* sock, int64_t session_id,
            int64_t stream_id)
            : sock(sock), session_id(session_id), stream_id(stream_id) {
        sock->ref();
    }

    DLLLOCAL ~QoreSocketObjectQuicStreamDataPollOperation() override {
        ExceptionSink xsink;
        cleanup(&xsink);
        if (xsink) {
            xsink.clear();
        }
        sock->deref(&xsink);
        if (xsink) {
            xsink.clear();
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink* xsink) override {
        done = true;
        data.discard();
        cleanup(xsink);
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (done) {
            return nullptr;
        }

        int rc = checkData(xsink);
        if (rc != 1) {
            cleanup(xsink);
            return nullptr;
        }

        if (!registered && registerWaiter(xsink)) {
            done = true;
            cleanup(xsink);
            return nullptr;
        }

        rc = checkData(xsink);
        if (rc != 1) {
            cleanup(xsink);
            return nullptr;
        }

        if (!wake_requested) {
            wake_requested = true;
            wakeIoThread(xsink);
            if (*xsink) {
                done = true;
                cleanup(xsink);
                return nullptr;
            }
        }

        return getSocketPollInfoHash(xsink, SOCK_POLLIN);
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return data ? data->refSelf() : QoreValue();
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        return done ? "quic-stream-data-read" : "waiting-quic-stream-data";
    }

    DLLLOCAL BinaryNode* takeOutput() {
        return data.release();
    }

private:
    DLLLOCAL bool ensureSession(ExceptionSink* xsink) {
        if (session) {
            return true;
        }
        session = qore_socket_object_get_quic_session(sock, session_id);
        if (!session) {
            done = true;
            xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
                (long long)session_id);
            return false;
        }
        return true;
    }

    DLLLOCAL int checkData(ExceptionSink* xsink) {
        if (!ensureSession(xsink)) {
            return -1;
        }
        if (session->isMarkedClosed()) {
            done = true;
            xsink->raiseException("QUIC-ERROR",
                "QUIC session closed during stream read (session %lld, stream %lld)",
                (long long)session_id, (long long)stream_id);
            return -1;
        }

        bool complete = false;
        QoreValue chunk = session->takeStreamData(stream_id, complete);
        if (chunk.getType() == NT_BINARY) {
            data = chunk.take<BinaryNode>();
            done = true;
            return 0;
        }
        if (complete) {
            done = true;
            return 0;
        }
        return 1;
    }

    DLLLOCAL int registerWaiter(ExceptionSink* xsink) {
        if (!ensureSession(xsink)) {
            return -1;
        }
        ReferenceHolder<QoreObject> obj(getReferencedSocketObject(xsink), xsink);
        if (*xsink) {
            return -1;
        }
        session->registerStreamDataWaiter(*obj, xsink);
        if (*xsink) {
            return -1;
        }
        waiter_sock_obj = obj.release();
        registered = true;
        return 0;
    }

    DLLLOCAL void cleanup(ExceptionSink* xsink) {
        if (registered && waiter_sock_obj && session) {
            session->unregisterStreamDataWaiter(waiter_sock_obj, xsink);
            registered = false;
        }
        if (waiter_sock_obj) {
            waiter_sock_obj->deref(xsink);
            waiter_sock_obj = nullptr;
        }
    }

    QoreSocketObject* sock;
    std::shared_ptr<QuicSession> session;
    SimpleRefHolder<BinaryNode> data;
    QoreObject* waiter_sock_obj = nullptr;
    int64_t session_id;
    int64_t stream_id;
    bool done = false;
    bool registered = false;
    bool wake_requested = false;
};

class QoreSocketObjectQuicDatagramPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketObjectQuicDatagramPollOperation(QoreSocketObject* sock, int64_t session_id,
            int64_t stream_id)
            : sock(sock), session_id(session_id), stream_id(stream_id) {
        sock->ref();
    }

    DLLLOCAL ~QoreSocketObjectQuicDatagramPollOperation() override {
        ExceptionSink xsink;
        cleanup(&xsink);
        if (xsink) {
            xsink.clear();
        }
        sock->deref(&xsink);
        if (xsink) {
            xsink.clear();
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink* xsink) override {
        done = true;
        data.discard();
        cleanup(xsink);
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (done) {
            return nullptr;
        }

        int rc = checkData(xsink);
        if (rc != 1) {
            cleanup(xsink);
            return nullptr;
        }

        if (!registered && registerWaiter(xsink)) {
            done = true;
            cleanup(xsink);
            return nullptr;
        }

        rc = checkData(xsink);
        if (rc != 1) {
            cleanup(xsink);
            return nullptr;
        }

        if (!wake_requested) {
            wake_requested = true;
            wakeIoThread(xsink);
            if (*xsink) {
                done = true;
                cleanup(xsink);
                return nullptr;
            }
        }

        return getSocketPollInfoHash(xsink, SOCK_POLLIN);
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return data ? data->refSelf() : QoreValue();
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        return done ? "quic-datagram-read" : "waiting-quic-datagram";
    }

    DLLLOCAL QoreValue takeOutput() {
        return data ? QoreValue(data.release()) : QoreValue();
    }

private:
    DLLLOCAL bool ensureSession(ExceptionSink* xsink) {
        if (session) {
            return true;
        }
        session = qore_socket_object_get_quic_session(sock, session_id);
        if (!session) {
            done = true;
            xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
                (long long)session_id);
            return false;
        }
        return true;
    }

    DLLLOCAL int checkData(ExceptionSink* xsink) {
        if (!ensureSession(xsink)) {
            return -1;
        }
        if (session->isMarkedClosed()) {
            done = true;
            return 0;
        }

        BinaryNode* chunk = session->takeDatagram(stream_id, xsink);
        if (*xsink) {
            done = true;
            return -1;
        }
        if (chunk) {
            data = chunk;
            done = true;
            return 0;
        }
        return 1;
    }

    DLLLOCAL int registerWaiter(ExceptionSink* xsink) {
        if (!ensureSession(xsink)) {
            return -1;
        }
        ReferenceHolder<QoreObject> obj(getReferencedSocketObject(xsink), xsink);
        if (*xsink) {
            return -1;
        }
        session->registerDatagramWaiter(*obj, xsink);
        if (*xsink) {
            return -1;
        }
        waiter_sock_obj = obj.release();
        registered = true;
        return 0;
    }

    DLLLOCAL void cleanup(ExceptionSink* xsink) {
        if (registered && waiter_sock_obj && session) {
            session->unregisterDatagramWaiter(waiter_sock_obj, xsink);
            registered = false;
        }
        if (waiter_sock_obj) {
            waiter_sock_obj->deref(xsink);
            waiter_sock_obj = nullptr;
        }
    }

    QoreSocketObject* sock;
    std::shared_ptr<QuicSession> session;
    SimpleRefHolder<BinaryNode> data;
    QoreObject* waiter_sock_obj = nullptr;
    int64_t session_id;
    int64_t stream_id;
    bool done = false;
    bool registered = false;
    bool wake_requested = false;
};

class QoreSocketObjectQuicConnectStreamDataPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketObjectQuicConnectStreamDataPollOperation(QoreSocketObject* sock, int64_t session_id,
            int64_t stream_id)
            : sock(sock), session_id(session_id), stream_id(stream_id) {
        sock->ref();
    }

    DLLLOCAL ~QoreSocketObjectQuicConnectStreamDataPollOperation() override {
        ExceptionSink xsink;
        sock->deref(&xsink);
        if (xsink) {
            xsink.clear();
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink*) override {
        done = true;
        data.discard();
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (done) {
            return nullptr;
        }

        std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(sock, session_id);
        if (!session) {
            xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
                (long long)session_id);
            done = true;
            return nullptr;
        }

        QoreValue rv = session->readConnectStreamData(stream_id, xsink);
        if (*xsink) {
            done = true;
            return nullptr;
        }
        if (rv.getType() == NT_BINARY) {
            data = rv.take<BinaryNode>();
        } else if (!rv.isNothing()) {
            std::string type = rv.getFullTypeName();
            rv.discard(xsink);
            if (!*xsink) {
                xsink->raiseException("QUIC-ERROR", "unexpected %s value reading QUIC CONNECT stream data",
                    type.c_str());
            }
        }
        done = true;
        return nullptr;
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return data ? data->refSelf() : QoreValue();
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        return done ? "quic-connect-stream-data-read" : "reading-quic-connect-stream-data";
    }

    DLLLOCAL QoreValue takeOutput() {
        return data ? QoreValue(data.release()) : QoreValue();
    }

private:
    QoreSocketObject* sock;
    SimpleRefHolder<BinaryNode> data;
    int64_t session_id;
    int64_t stream_id;
    bool done = false;
};

class QoreSocketObjectQuicQueryPollOperation : public SocketPollOperationBase {
public:
    enum class Action {
        FirstSessionClosed,
        FirstSessionId,
        GoawayState,
        GoawayReceived,
        PeerCertificate,
        StreamComplete,
        MaxDatagramSize,
        DatagramSupported,
    };

    DLLLOCAL QoreSocketObjectQuicQueryPollOperation(QoreSocketObject* sock, Action action,
            int64_t session_id = 0, int64_t stream_id = 0)
            : sock(sock), action(action), session_id(session_id), stream_id(stream_id) {
        sock->ref();
    }

    DLLLOCAL ~QoreSocketObjectQuicQueryPollOperation() override {
        ExceptionSink xsink;
        output.discard(&xsink);
        if (xsink) {
            xsink.clear();
        }
        sock->deref(&xsink);
        if (xsink) {
            xsink.clear();
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink* xsink) override {
        output.discard(xsink);
        output = defaultValue();
        done = true;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (done) {
            return nullptr;
        }

        switch (action) {
            case Action::FirstSessionClosed:
                setFirstSessionClosed();
                break;

            case Action::FirstSessionId:
                setFirstSessionId(xsink);
                break;

            case Action::GoawayState:
                setGoawayState(xsink);
                break;

            case Action::GoawayReceived:
                setGoawayReceived();
                break;

            case Action::PeerCertificate:
                setPeerCertificate(xsink);
                break;

            case Action::StreamComplete:
                setStreamComplete();
                break;

            case Action::MaxDatagramSize:
                setMaxDatagramSize(xsink);
                break;

            case Action::DatagramSupported:
                setDatagramSupported(xsink);
                break;
        }

        if (*xsink) {
            output.discard(xsink);
            output = defaultValue();
        }
        done = true;
        return nullptr;
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return output.refSelf();
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        return done ? "quic-query-done" : "querying-quic-session";
    }

private:
    DLLLOCAL QoreValue defaultValue() const {
        switch (action) {
            case Action::FirstSessionClosed:
            case Action::StreamComplete:
                return true;
            case Action::FirstSessionId:
            case Action::MaxDatagramSize:
                return 0;
            case Action::GoawayReceived:
            case Action::DatagramSupported:
                return false;
            case Action::GoawayState:
            case Action::PeerCertificate:
                return QoreValue();
        }
        return QoreValue();
    }

    DLLLOCAL std::shared_ptr<QuicSession> getSession(ExceptionSink* xsink, const char* err,
            const char* desc) const {
        std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(sock, session_id);
        if (!session) {
            xsink->raiseException(err, desc, (long long)session_id);
        }
        return session;
    }

    DLLLOCAL void setFirstSessionClosed() {
        std::shared_ptr<QuicSession> session = qore_socket_object_get_first_quic_session(sock);
        output = !session || session->isClosed();
    }

    DLLLOCAL void setFirstSessionId(ExceptionSink* xsink) {
        my_socket_priv* priv = my_socket_priv::getPriv(*sock);
        AutoLocker al(priv->m);
        qore_socket_private* sp = qore_socket_private::get(*priv->socket);
        output = sp->getFirstQuicSessionId(xsink);
    }

    DLLLOCAL void setGoawayState(ExceptionSink* xsink) {
        std::shared_ptr<QuicSession> session = getSession(xsink, "QUIC-SESSION-ERROR",
            "session %lld not found");
        if (*xsink) {
            return;
        }
        ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclQuicGoawayStateInfo, xsink), xsink);
        if (*xsink) {
            return;
        }
        h->setKeyValue("goaway_sent", session->isGoawaySent(), xsink);
        h->setKeyValue("goaway_received", session->isGoawayReceived(), xsink);
        h->setKeyValue("goaway_max_stream_id", session->getGoawayMaxStreamId(), xsink);
        if (!*xsink) {
            output = h.release();
        }
    }

    DLLLOCAL void setGoawayReceived() {
        std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(sock, session_id);
        output = session ? session->isGoawayReceived() : false;
    }

    DLLLOCAL void setPeerCertificate(ExceptionSink* xsink) {
        std::shared_ptr<QuicSession> session = getSession(xsink, "QUIC-SESSION-ERROR",
            "no QUIC session with id %lld on this socket");
        if (*xsink) {
            return;
        }
        X509* cert = session->getPeerCertificate();
        if (cert) {
            output = new QoreObject(QC_SSLCERTIFICATE, getProgram(), new QoreSSLCertificate(cert));
        }
    }

    DLLLOCAL void setStreamComplete() {
        std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(sock, session_id);
        output = !session || session->isStreamComplete(stream_id);
    }

    DLLLOCAL void setMaxDatagramSize(ExceptionSink* xsink) {
        std::shared_ptr<QuicSession> session = getSession(xsink, "QUIC-ERROR",
            "no QUIC session with id %lld on this socket");
        if (*xsink) {
            return;
        }
        output = static_cast<int64_t>(session->getMaxDatagramPayloadSize(stream_id));
    }

    DLLLOCAL void setDatagramSupported(ExceptionSink* xsink) {
        std::shared_ptr<QuicSession> session = getSession(xsink, "QUIC-ERROR",
            "no QUIC session with id %lld on this socket");
        if (*xsink) {
            return;
        }
        output = session->isDatagramSupported();
    }

    QoreSocketObject* sock;
    QoreValue output;
    Action action;
    int64_t session_id;
    int64_t stream_id;
    bool done = false;
};

static QoreObject* qore_socket_object_make_pollable_wrapper(QoreSocketObject* s) {
    s->ref();
    return new QoreObject(QC_ABSTRACTPOLLABLEIOOBJECTBASE, getProgram(), s);
}

static void qore_socket_object_wake_async_controller(QoreSocketObject* s, ExceptionSink* xsink) {
    if (qore_on_async_io_thread()) {
        return;
    }

    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    if (*xsink || !sock_obj) {
        return;
    }
    ReferenceHolder<QoreObject> ctl_obj(qore_get_async_io_controller_obj(xsink), xsink);
    if (*xsink || !ctl_obj) {
        return;
    }
    ReferenceHolder<AsyncIoControllerPriv> ctl_priv(
        static_cast<AsyncIoControllerPriv*>(
            (*ctl_obj)->getReferencedPrivateData(CID_ASYNCIOCONTROLLER, xsink)), xsink);
    if (*xsink || !ctl_priv) {
        return;
    }
    ctl_priv->wakeSocketByObject(*sock_obj, xsink);
}

static int qore_socket_object_parse_http2_request_headers(const QoreHashNode* headers, std::string& method,
        std::string& path, std::vector<std::pair<std::string, std::string>>& request_headers,
        ExceptionSink* xsink) {
    if (!headers) {
        xsink->raiseException("HTTP2-ERROR", "HTTP/2 request headers are required");
        return -1;
    }

    request_headers.reserve(headers->size() + 4);
    auto append = [&](const std::string& key, const char* sval) {
        if (key == ":method") {
            if (sval && *sval) {
                method = sval;
            }
        } else if (key == ":path") {
            if (sval && *sval) {
                path = sval;
            }
        }
        request_headers.emplace_back(key, sval ? sval : "");
    };

    ConstHashIterator hi(headers);
    while (hi.next()) {
        const char* key = hi.getKey();
        QoreValue val = hi.get();
        std::string skey(key);
        if (val.getType() == NT_STRING) {
            append(skey, val.get<const QoreStringNode>()->c_str());
        } else if (val.getType() == NT_LIST) {
            const QoreListNode* l = val.get<const QoreListNode>();
            for (size_t i = 0; i < l->size(); ++i) {
                QoreValue elem = l->retrieveEntry(i);
                if (elem.getType() == NT_STRING) {
                    append(skey, elem.get<const QoreStringNode>()->c_str());
                }
            }
        }
    }

    if (method.empty()) {
        xsink->raiseException("HTTP2-ERROR", "HTTP/2 request ':method' header is missing or empty");
        return -1;
    }
    if (path.empty()) {
        xsink->raiseException("HTTP2-ERROR", "HTTP/2 request ':path' header is missing or empty");
        return -1;
    }

    return 0;
}

static std::shared_ptr<QuicSession> qore_socket_object_get_quic_session(const QoreSocketObject* s,
        int64_t session_id) {
    my_socket_priv* priv = my_socket_priv::getPriv(*const_cast<QoreSocketObject*>(s));
    AutoLocker al(priv->m);
    qore_socket_private* sp = qore_socket_private::get(*priv->socket);
    return sp->getQuicSession(session_id);
}

static std::shared_ptr<QuicSession> qore_socket_object_get_first_quic_session(const QoreSocketObject* s) {
    my_socket_priv* priv = my_socket_priv::getPriv(*const_cast<QoreSocketObject*>(s));
    AutoLocker al(priv->m);
    qore_socket_private* sp = qore_socket_private::get(*priv->socket);
    AutoLocker al2(sp->quic_sessions_lock);
    if (sp->quic_sessions.empty()) {
        return nullptr;
    }
    return sp->quic_sessions.begin()->second;
}

static Http2SessionPtr qore_socket_object_get_h2_session(const QoreSocketObject* s) {
    my_socket_priv* priv = my_socket_priv::getPriv(*const_cast<QoreSocketObject*>(s));
    AutoLocker al(priv->m);
    qore_socket_private* sp = qore_socket_private::get(*priv->socket);
    return sp->h2_session;
}

class QoreSocketObjectHttp2StreamDataPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketObjectHttp2StreamDataPollOperation(QoreSocketObject* sock, int32_t stream_id,
            size_t max_bytes)
            : sock(sock), stream_id(stream_id), max_bytes(max_bytes) {
        sock->ref();
    }

    DLLLOCAL ~QoreSocketObjectHttp2StreamDataPollOperation() override {
        ExceptionSink xsink;
        sock->deref(&xsink);
        if (xsink) {
            xsink.clear();
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink*) override {
        done = true;
        data.discard();
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (done) {
            return nullptr;
        }

        Http2SessionPtr h2 = qore_socket_object_get_h2_session(sock);
        if (!h2) {
            xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
            done = true;
            return nullptr;
        }

        data = h2->takeStreamData(stream_id, max_bytes, xsink);
        done = true;
        return nullptr;
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return data ? data->refSelf() : QoreValue();
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        return done ? "http2-stream-data-read" : "reading-http2-stream-data";
    }

    DLLLOCAL BinaryNode* takeOutput() {
        return data.release();
    }

private:
    QoreSocketObject* sock;
    SimpleRefHolder<BinaryNode> data;
    int32_t stream_id;
    size_t max_bytes;
    bool done = false;
};

class QoreSocketObjectHttp2StreamStatePollOperation : public SocketPollOperationBase {
public:
    enum class Action {
        Closed,
        RemoteClosed,
    };

    DLLLOCAL QoreSocketObjectHttp2StreamStatePollOperation(QoreSocketObject* sock, int32_t stream_id,
            Action action)
            : sock(sock), stream_id(stream_id), action(action) {
        sock->ref();
    }

    DLLLOCAL ~QoreSocketObjectHttp2StreamStatePollOperation() override {
        ExceptionSink xsink;
        sock->deref(&xsink);
        if (xsink) {
            xsink.clear();
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink*) override {
        output = true;
        done = true;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink*) override {
        if (done) {
            return nullptr;
        }

        Http2SessionPtr h2 = qore_socket_object_get_h2_session(sock);
        if (!h2) {
            output = true;
        } else if (action == Action::Closed) {
            output = h2->isStreamClosed(stream_id);
        } else {
            output = h2->isStreamRemoteClosed(stream_id);
        }
        done = true;
        return nullptr;
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return output;
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        return done ? "http2-stream-state-checked" : "checking-http2-stream-state";
    }

private:
    QoreSocketObject* sock;
    int32_t stream_id;
    Action action;
    bool output = true;
    bool done = false;
};

static void qore_socket_object_set_h2_headers(strcase_str_map_t& out, const QoreHashNode* headers) {
    if (!headers) {
        return;
    }

    ConstHashIterator hi(headers);
    while (hi.next()) {
        QoreValue val = hi.get();
        if (val.getType() == NT_STRING) {
            out[hi.getKey()] = val.get<const QoreStringNode>()->c_str();
        }
    }
}

static void qore_socket_object_set_quic_headers(strcase_str_map_t& out, const QoreHashNode* headers) {
    if (!headers) {
        return;
    }

    ConstHashIterator hi(headers);
    while (hi.next()) {
        QoreStringValueHelper val(hi.get());
        out[hi.getKey()] = val->c_str();
    }
}

static QoreHashNode* qore_socket_object_make_sockaddr_output(const struct sockaddr_storage& addr, socklen_t len,
        const std::string& socketname, const std::string& hostname = std::string()) {
    QoreHashNode* h = new QoreHashNode(autoTypeInfo);
    BinaryNode* b = new BinaryNode();
    b->append(&addr, sizeof(addr));
    h->setKeyValue("addr", b, nullptr);
    h->setKeyValue("len", static_cast<int64>(len), nullptr);
    if (!socketname.empty()) {
        h->setKeyValue("socketname", new QoreStringNode(socketname), nullptr);
    }
    if (!hostname.empty()) {
        h->setKeyValue("hostname", new QoreStringNode(hostname), nullptr);
    }
    return h;
}

class QoreSocketObjectHttp2EnqueuePollOperation : public SocketPollOperationBase {
public:
    enum class Action {
        PushPromise,
        Response,
        ConnectResponse,
        Request,
        Cancel,
        StreamData,
        Trailers,
        StreamingResponseHeaders,
        StreamingResponseWithStream,
        Cleanup,
        Reset,
        SetStreaming,
    };

    DLLLOCAL QoreSocketObjectHttp2EnqueuePollOperation(QoreSocketObject* sock, Action action,
            int32_t stream_id, const char* path, const QoreHashNode* headers)
            : sock(sock), action(action), stream_id(stream_id), path(path ? path : "") {
        sock->ref();
        qore_socket_object_set_h2_headers(header_map, headers);
    }

    DLLLOCAL QoreSocketObjectHttp2EnqueuePollOperation(QoreSocketObject* sock, Action action,
            int32_t stream_id, int status_code, const QoreHashNode* headers, const void* body_ptr = nullptr,
            size_t body_len = 0)
            : sock(sock), action(action), stream_id(stream_id), status_code(status_code) {
        sock->ref();
        qore_socket_object_set_h2_headers(header_map, headers);
        if (body_ptr && body_len) {
            body = new BinaryNode;
            body->append(body_ptr, body_len);
        }
    }

    DLLLOCAL QoreSocketObjectHttp2EnqueuePollOperation(QoreSocketObject* sock, const QoreHashNode* headers,
            const void* body_ptr, size_t body_len, bool streaming, bool wake_controller, ExceptionSink* xsink)
            : sock(sock), action(Action::Request), streaming(streaming), wake_controller(wake_controller) {
        sock->ref();
        parseRequestHeaders(headers, xsink);
        if (body_ptr && body_len) {
            body = new BinaryNode;
            body->append(body_ptr, body_len);
        }
    }

    DLLLOCAL QoreSocketObjectHttp2EnqueuePollOperation(QoreSocketObject* sock, int32_t stream_id,
            const BinaryNode* data, bool end_stream)
            : sock(sock), action(Action::StreamData), stream_id(stream_id), end_stream(end_stream) {
        sock->ref();
        if (data && data->size()) {
            body = new BinaryNode;
            body->append(data->getPtr(), data->size());
        }
    }

    DLLLOCAL QoreSocketObjectHttp2EnqueuePollOperation(QoreSocketObject* sock, Action action,
            int32_t stream_id, bool wake_controller = true)
            : sock(sock), action(action), stream_id(stream_id), wake_controller(wake_controller) {
        sock->ref();
    }

    DLLLOCAL QoreSocketObjectHttp2EnqueuePollOperation(QoreSocketObject* sock, Action action,
            int32_t stream_id, uint32_t error_code, bool wake_controller = true)
            : sock(sock), action(action), stream_id(stream_id), h2_error_code(error_code),
            wake_controller(wake_controller) {
        assert(action == Action::Reset || action == Action::Cancel);
        sock->ref();
    }

    DLLLOCAL QoreSocketObjectHttp2EnqueuePollOperation(QoreSocketObject* sock, int32_t stream_id,
            int status_code, const QoreHashNode* headers, InputStream* input_stream, int64_t content_length)
            : sock(sock), action(Action::StreamingResponseWithStream), stream_id(stream_id),
            status_code(status_code), input_stream(input_stream), content_length(content_length) {
        sock->ref();
        qore_socket_object_set_h2_headers(header_map, headers);
    }

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink*) override {
        output = -1;
        done = true;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (done) {
            return nullptr;
        }

        Http2SessionPtr h2 = qore_socket_object_get_h2_session(sock);
        if (!h2) {
            if (action == Action::Cleanup || action == Action::Reset || action == Action::SetStreaming) {
                output = 0;
                done = true;
                return nullptr;
            }
            xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
            output = -1;
            done = true;
            return nullptr;
        }

        switch (action) {
            case Action::PushPromise:
                output = h2->submitPushPromise(stream_id, path.c_str(), header_map, xsink);
                break;

            case Action::Response: {
                const void* ptr = body ? body->getPtr() : nullptr;
                size_t len = body ? body->size() : 0;
                output = h2->submitResponse(stream_id, status_code, header_map, ptr, len, xsink);
                break;
            }

            case Action::ConnectResponse:
                output = h2->submitConnectResponse(stream_id, status_code, header_map, xsink);
                break;

            case Action::Request: {
                const void* ptr = body ? body->getPtr() : nullptr;
                size_t len = body ? body->size() : 0;
                output = h2->submitRequest(method.c_str(), path.c_str(), request_headers, ptr, len, xsink,
                    streaming);
                break;
            }

            case Action::Cancel:
                output = h2->submitRstStream(stream_id, NGHTTP2_CANCEL, xsink);
                break;

            case Action::StreamData: {
                const void* ptr = body ? body->getPtr() : nullptr;
                size_t len = body ? body->size() : 0;
                int rv = h2->sendStreamData(stream_id, ptr, len, end_stream, xsink);
                if (rv > 0 && !*xsink) {
                    xsink->raiseException("HTTP2-FLOW-CONTROL", "stream %d buffer full: data dropped", stream_id);
                    output = -1;
                } else if (rv == 0) {
                    // Synchronously drive nghttp2 to serialize the just-enqueued
                    // chunk and push the bytes onto the socket BEFORE this
                    // poll-op completes.  Without this, sendStreamData() only
                    // appends to pending_body_data + flips the data provider
                    // out of the deferred state — the actual DATA frame isn't
                    // emitted until the next I/O loop iteration that processes
                    // the wakeSocket() command.  For SSE / event-stream flows
                    // emitting one event per multi-second interval, that
                    // round-trip latency was empirically observed as a 30+
                    // minute "all events dump on response close" symptom: each
                    // chunk waits for the next event to provoke a wake.
                    // Driving sendPendingData here matches the H1 chunked-body
                    // sender's per-chunk synchronous flush
                    // (lib/QoreSocketObject.cpp:4031 path).
                    //
                    // Safe lock-wise: sendPendingData acquires the same
                    // recursive Http2Session mutex that sendStreamData just
                    // released.  Safe I/O-wise: the inner socket write uses
                    // the existing OptionalNonBlockingHelper, so a partial
                    // write on a blocked socket leaves the remainder in
                    // send_buffer for the next I/O loop drain — the wake the
                    // caller eventually issues will still flush it.
                    if (h2->sendPendingData(0, xsink) < 0) {
                        output = -1;
                    } else {
                        output = 0;
                    }
                } else {
                    output = rv;
                }
                break;
            }

            case Action::Trailers:
                output = h2->submitTrailers(stream_id, header_map, xsink);
                break;

            case Action::StreamingResponseHeaders:
                output = h2->submitResponseStreaming(stream_id, status_code, header_map, xsink);
                break;

            case Action::StreamingResponseWithStream:
                output = h2->submitResponseStreaming(stream_id, status_code, header_map, xsink);
                if (!output && !*xsink) {
                    h2->setStreamInputStream(stream_id, input_stream, xsink, content_length);
                    if (!*xsink) {
                        input_stream = nullptr;
                    }
                }
                break;

            case Action::Cleanup:
                h2->cleanupStream(stream_id);
                output = 0;
                break;

            case Action::Reset:
                output = h2->submitRstStream(stream_id, h2_error_code, xsink);
                if (output != 0) {
                    printd(2, "resetHttp2StreamAsync() submitRstStream failed for stream %d (rv=%d), "
                        "cleaning up local state anyway\n", stream_id, (int)output);
                }
                h2->cleanupStream(stream_id);
                break;

            case Action::SetStreaming:
                h2->setStreamStreaming(stream_id);
                output = 0;
                break;
        }

        if (!*xsink && output >= 0 && wake_controller) {
            wakeIoThread(xsink);
        }
        if (*xsink) {
            output = -1;
        }
        done = true;
        return nullptr;
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return static_cast<int64>(output);
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        return done ? "done" : "enqueueing";
    }

private:
    DLLLOCAL void parseRequestHeaders(const QoreHashNode* headers, ExceptionSink* xsink) {
        qore_socket_object_parse_http2_request_headers(headers, method, path, request_headers, xsink);
    }

    QoreSocketObject* sock;
    Action action;
    int32_t stream_id = 0;
    int status_code = 0;
    uint32_t h2_error_code = NGHTTP2_CANCEL;
    std::string method;
    std::string path;
    strcase_str_map_t header_map;
    std::vector<std::pair<std::string, std::string>> request_headers;
    SimpleRefHolder<BinaryNode> body;
    InputStream* input_stream = nullptr;
    int64_t content_length = -1;
    bool end_stream = false;
    bool streaming = false;
    bool wake_controller = true;
    int64 output = -1;
    bool done = false;
};

class QoreSocketObjectQuicEnqueuePollOperation : public SocketPollOperationBase {
public:
    enum class Action {
        Request,
        RequestStreaming,
        ClientStreamData,
        Cancel,
        Response,
        ShutdownNotice,
        Shutdown,
        ResponseStreaming,
        StreamData,
        SetStreamInputStream,
        StreamingResponseWithStream,
        ConnectResponse,
        Cleanup,
        Reset,
        Datagram,
        Trailers,
        SetStreaming,
        RegisterConnectQueue,
        DeregisterConnectQueue,
        RegisterConnectFrameState,
        DeregisterConnectFrameState,
        RegisterDatagramQueue,
        UnregisterDatagramQueue,
    };

    DLLLOCAL QoreSocketObjectQuicEnqueuePollOperation(QoreSocketObject* sock, const char* method,
            const char* path, const QoreHashNode* headers, const void* body_ptr, size_t body_len,
            bool streaming, bool wake_controller = true)
            : sock(sock), action(streaming ? Action::RequestStreaming : Action::Request),
            method(method ? method : ""), path(path ? path : ""), use_first_session(true),
            wake_controller(wake_controller) {
        sock->ref();
        qore_socket_object_set_quic_headers(header_map, headers);
        setBody(body_ptr, body_len);
    }

    DLLLOCAL QoreSocketObjectQuicEnqueuePollOperation(QoreSocketObject* sock, Action action,
            int64_t session_id, int64_t stream_id, const void* body_ptr, size_t body_len, bool end_stream)
            : sock(sock), action(action), session_id(session_id), stream_id(stream_id),
            end_stream(end_stream) {
        assert(action == Action::ClientStreamData || action == Action::StreamData || action == Action::Datagram);
        sock->ref();
        use_first_session = action == Action::ClientStreamData;
        setBody(body_ptr, body_len);
    }

    DLLLOCAL QoreSocketObjectQuicEnqueuePollOperation(QoreSocketObject* sock, Action action,
            int64_t session_id, int64_t stream_id, int status_code, const QoreHashNode* headers,
            const void* body_ptr = nullptr, size_t body_len = 0)
            : sock(sock), action(action), session_id(session_id), stream_id(stream_id),
            status_code(status_code) {
        assert(action == Action::Response || action == Action::ResponseStreaming
            || action == Action::ConnectResponse || action == Action::Trailers);
        sock->ref();
        use_first_session = action == Action::Trailers;
        qore_socket_object_set_quic_headers(header_map, headers);
        setBody(body_ptr, body_len);
    }

    DLLLOCAL QoreSocketObjectQuicEnqueuePollOperation(QoreSocketObject* sock, Action action,
            int64_t session_id, int64_t stream_id = 0)
            : sock(sock), action(action), session_id(session_id), stream_id(stream_id) {
        assert(action == Action::Cancel || action == Action::ShutdownNotice || action == Action::Shutdown
            || action == Action::Cleanup || action == Action::Reset || action == Action::SetStreaming
            || action == Action::DeregisterConnectQueue || action == Action::DeregisterConnectFrameState
            || action == Action::UnregisterDatagramQueue);
        sock->ref();
        use_first_session = action == Action::SetStreaming;
    }

    DLLLOCAL QoreSocketObjectQuicEnqueuePollOperation(QoreSocketObject* sock, Action action,
            int64_t session_id, int64_t stream_id, Queue* queue)
            : sock(sock), action(action), session_id(session_id), stream_id(stream_id), queue(queue) {
        assert(action == Action::RegisterConnectQueue || action == Action::RegisterConnectFrameState
            || action == Action::RegisterDatagramQueue);
        sock->ref();
    }

    DLLLOCAL QoreSocketObjectQuicEnqueuePollOperation(QoreSocketObject* sock, int64_t session_id,
            int64_t stream_id, InputStream* input_stream, bool ref_input_stream)
            : sock(sock), action(Action::SetStreamInputStream), session_id(session_id), stream_id(stream_id) {
        sock->ref();
        setInputStream(input_stream, ref_input_stream);
    }

    DLLLOCAL QoreSocketObjectQuicEnqueuePollOperation(QoreSocketObject* sock, int64_t session_id,
            int64_t stream_id, int status_code, const QoreHashNode* headers, InputStream* input_stream,
            bool ref_input_stream)
            : sock(sock), action(Action::StreamingResponseWithStream), session_id(session_id),
            stream_id(stream_id), status_code(status_code) {
        sock->ref();
        qore_socket_object_set_quic_headers(header_map, headers);
        setInputStream(input_stream, ref_input_stream);
    }

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (queue) {
                queue->deref(xsink);
                queue = nullptr;
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink*) override {
        output = -1;
        done = true;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (done) {
            return nullptr;
        }

        std::shared_ptr<QuicSession> session = getSession(xsink);
        if (*xsink || !session) {
            output = sessionMissingIsOk() ? 0 : -1;
            done = true;
            return nullptr;
        }

        switch (action) {
            case Action::Request:
                output = session->submitRequest(method.c_str(), path.c_str(), header_map,
                    body ? body->getPtr() : nullptr, body ? body->size() : 0, xsink);
                break;

            case Action::RequestStreaming:
                output = session->submitRequestStreaming(method.c_str(), path.c_str(), header_map, xsink);
                break;

            case Action::ClientStreamData:
            case Action::StreamData:
                output = session->sendStreamData(stream_id, body ? body->getPtr() : nullptr,
                    body ? body->size() : 0, end_stream, xsink);
                break;

            case Action::Cancel:
                output = session->cancelStream(stream_id, NGHTTP3_H3_REQUEST_CANCELLED, xsink);
                break;

            case Action::Response:
                output = session->submitResponse(stream_id, status_code, header_map,
                    body ? body->getPtr() : nullptr, body ? body->size() : 0, xsink);
                break;

            case Action::ShutdownNotice:
                output = session->submitShutdownNotice(xsink);
                break;

            case Action::Shutdown:
                output = session->submitShutdown(xsink);
                break;

            case Action::ResponseStreaming:
                output = session->submitResponseStreaming(stream_id, status_code, header_map, xsink);
                break;

            case Action::SetStreamInputStream:
                session->setStreamInputStream(stream_id, *input_stream, xsink);
                if (!*xsink) {
                    input_stream.release();
                    output = 0;
                }
                break;

            case Action::StreamingResponseWithStream:
                output = session->submitResponseStreaming(stream_id, status_code, header_map, xsink);
                if (!output && !*xsink) {
                    session->setStreamInputStream(stream_id, *input_stream, xsink);
                    if (!*xsink) {
                        input_stream.release();
                        output = 0;
                    }
                }
                break;

            case Action::ConnectResponse:
                output = session->submitConnectResponse(stream_id, status_code, header_map, xsink);
                break;

            case Action::Cleanup:
                session->cleanupStream(stream_id);
                output = 0;
                break;

            case Action::Reset:
                output = session->resetStream(stream_id);
                break;

            case Action::Datagram:
                output = session->submitDatagram(stream_id,
                    body ? reinterpret_cast<const uint8_t*>(body->getPtr()) : nullptr,
                    body ? body->size() : 0, xsink);
                break;

            case Action::Trailers:
                output = session->submitTrailers(stream_id, header_map, xsink);
                break;

            case Action::SetStreaming:
                session->setStreamStreaming(stream_id);
                output = 0;
                break;

            case Action::RegisterConnectQueue:
                session->registerConnectStreamQueue(stream_id, queue);
                queue = nullptr;
                output = 0;
                break;

            case Action::DeregisterConnectQueue:
                session->deregisterConnectStreamQueue(stream_id);
                output = 0;
                break;

            case Action::RegisterConnectFrameState:
                session->registerConnectStreamFrameState(stream_id, queue);
                queue = nullptr;
                output = 0;
                break;

            case Action::DeregisterConnectFrameState:
                session->deregisterConnectStreamFrameState(stream_id);
                output = 0;
                break;

            case Action::RegisterDatagramQueue:
                session->registerDatagramQueue(stream_id, queue, xsink);
                if (!*xsink) {
                    queue = nullptr;
                    output = 0;
                }
                break;

            case Action::UnregisterDatagramQueue:
                session->unregisterDatagramQueue(stream_id, xsink);
                if (!*xsink) {
                    output = 0;
                }
                break;
        }

        if (!*xsink && shouldWakeController()) {
            wakeIoThread(xsink);
        }
        if (*xsink) {
            output = -1;
        }
        done = true;
        return nullptr;
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return static_cast<int64>(output);
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        return done ? "done" : "quic-enqueueing";
    }

private:
    DLLLOCAL void setBody(const void* body_ptr, size_t body_len) {
        if (body_ptr && body_len) {
            body = new BinaryNode;
            body->append(body_ptr, body_len);
        }
    }

    DLLLOCAL void setInputStream(InputStream* is, bool add_ref) {
        if (is && add_ref) {
            is->ref();
        }
        input_stream = is;
    }

    DLLLOCAL std::shared_ptr<QuicSession> getSession(ExceptionSink* xsink) const {
        if (use_first_session) {
            std::shared_ptr<QuicSession> session = qore_socket_object_get_first_quic_session(sock);
            if (!session) {
                if (action == Action::Request || action == Action::RequestStreaming) {
                    xsink->raiseException("QUIC-ERROR", "no active QUIC session on this socket; "
                        "use startPollQuicConnect() first");
                } else {
                    xsink->raiseException("QUIC-ERROR", "no active QUIC session on this socket");
                }
            }
            return session;
        }

        std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(sock, session_id);
        if (!session) {
            if (sessionMissingIsOk()) {
                return nullptr;
            } else if (action == Action::ShutdownNotice || action == Action::Shutdown) {
                xsink->raiseException("QUIC-SESSION-ERROR", "session %lld not found",
                    (long long)session_id);
            } else {
                xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
                    (long long)session_id);
            }
        }
        return session;
    }

    DLLLOCAL bool shouldWakeController() const {
        if (!wake_controller) {
            return false;
        }
        switch (action) {
            case Action::Request:
            case Action::RequestStreaming:
                return output >= 0;
            case Action::ClientStreamData:
            case Action::StreamData:
            case Action::Cancel:
            case Action::Response:
            case Action::ShutdownNotice:
            case Action::Shutdown:
            case Action::ResponseStreaming:
            case Action::SetStreamInputStream:
            case Action::StreamingResponseWithStream:
            case Action::ConnectResponse:
            case Action::Reset:
            case Action::Datagram:
            case Action::Trailers:
                return output == 0;
            case Action::Cleanup:
            case Action::SetStreaming:
            case Action::RegisterConnectQueue:
            case Action::DeregisterConnectQueue:
            case Action::RegisterConnectFrameState:
            case Action::DeregisterConnectFrameState:
            case Action::RegisterDatagramQueue:
            case Action::UnregisterDatagramQueue:
                return false;
        }
        return false;
    }

    DLLLOCAL bool sessionMissingIsOk() const {
        return action == Action::DeregisterConnectQueue
            || action == Action::DeregisterConnectFrameState
            || action == Action::UnregisterDatagramQueue;
    }

    QoreSocketObject* sock;
    Action action;
    int64_t session_id = 0;
    int64_t stream_id = 0;
    int status_code = 0;
    std::string method;
    std::string path;
    strcase_str_map_t header_map;
    SimpleRefHolder<BinaryNode> body;
    SimpleRefHolder<InputStream> input_stream;
    Queue* queue = nullptr;
    bool end_stream = false;
    bool use_first_session = false;
    bool wake_controller = true;
    int64 output = -1;
    bool done = false;
};

static int64_t qore_socket_object_get_content_length(const QoreHashNode* headers) {
    if (!headers) {
        return -1;
    }

    ConstHashIterator hi(headers);
    while (hi.next()) {
        if (!strcasecmp(hi.getKey(), "content-length")) {
            QoreValue v = hi.get();
            if (v.getType() == NT_STRING) {
                QoreStringValueHelper str(v);
                const char* s = str->c_str();
                char* endptr = nullptr;
                long long cl = strtoll(s, &endptr, 10);
                if (endptr != s && cl >= 0) {
                    return cl;
                }
            } else if (v.getType() == NT_INT) {
                return v.getAsBigInt();
            }
            break;
        }
    }
    return -1;
}

class QoreSocketObjectAddressInfoPollOperation : public SocketPollOperationBase {
public:
    enum class Action {
        Peer,
        Socket,
    };

    DLLLOCAL QoreSocketObjectAddressInfoPollOperation(QoreSocketObject* sock, Action action, bool host_lookup)
            : sock(sock), action(action), host_lookup(host_lookup) {
    }

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink*) override {
        done = true;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (done) {
            return nullptr;
        }

        if (!success) {
            my_socket_priv* priv = my_socket_priv::getPriv(*sock);
            AutoLocker al(priv->m);
            if (priv->checkValid(xsink)) {
                done = true;
                return nullptr;
            }

            qore_socket_private* sp = qore_socket_private::get(*priv->socket);
            int rc = action == Action::Peer
                ? sp->getPeerSockAddr(xsink, addr, len)
                : sp->getSocketSockAddr(xsink, addr, len);
            if (rc) {
                done = true;
                return nullptr;
            }
            socketname = sp->socketname;
            success = true;
            if (host_lookup && (addr.ss_family == AF_INET || addr.ss_family == AF_INET6)) {
                resolver = std::make_unique<QoreCaresNameInfoResolver>(addr, len);
            } else {
                done = true;
                return nullptr;
            }
        }

        assert(resolver);
        int rc = resolver->continuePoll(xsink);
        if (*xsink || rc < 0) {
            done = true;
            resolver.reset();
            return nullptr;
        }
        if (rc) {
            return getResolverPollInfo(xsink);
        }

        hostname = resolver->getHostname();
        resolver.reset();
        done = true;
        return nullptr;
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return success ? qore_socket_object_make_sockaddr_output(addr, len, socketname, hostname) : QoreValue();
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        if (done) {
            return "done";
        }
        if (resolver) {
            return action == Action::Peer ? "resolving-peer-info" : "resolving-socket-info";
        }
        return action == Action::Peer ? "getting-peer-info" : "getting-socket-info";
    }

private:
    DLLLOCAL QoreHashNode* getResolverPollInfo(ExceptionSink* xsink) const {
        std::vector<std::pair<int, int>> extra_fds;
        resolver->getExtraFds(extra_fds);
        QoreHashNode* rv = getSocketPollInfoHash(xsink, 0, extra_fds);
        if (*xsink) {
            return nullptr;
        }
        int poll_timeout_ms = resolver->getPollTimeoutMs();
        rv->setKeyValue("poll_timeout_ms", poll_timeout_ms >= 0 ? poll_timeout_ms : 1, xsink);
        return rv;
    }

    QoreSocketObject* sock;
    Action action;
    bool host_lookup;
    std::unique_ptr<QoreCaresNameInfoResolver> resolver;
    struct sockaddr_storage addr = {};
    socklen_t len = 0;
    std::string socketname;
    std::string hostname;
    bool done = false;
    bool success = false;
};

class QoreSocketObjectTlsStatePollOperation : public SocketPollOperationBase {
public:
    enum class Action {
        GetCipherName,
        GetCipherVersion,
        IsSecure,
        SetAlpnProtocols,
        GetAlpnProtocol,
        IsHttp2,
        VerifyPeerCertificate,
        GetRemoteCertificate,
    };

    DLLLOCAL QoreSocketObjectTlsStatePollOperation(QoreSocketObject* sock, Action action)
            : sock(sock), action(action) {
        sock->ref();
    }

    DLLLOCAL QoreSocketObjectTlsStatePollOperation(QoreSocketObject* sock, std::vector<std::string>&& protocols)
            : sock(sock), action(Action::SetAlpnProtocols), protocols(std::move(protocols)) {
        sock->ref();
    }

    DLLLOCAL ~QoreSocketObjectTlsStatePollOperation() override {
        ExceptionSink xsink;
        output.discard(&xsink);
        if (xsink) {
            xsink.clear();
        }
        sock->deref(&xsink);
        if (xsink) {
            xsink.clear();
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink* xsink) override {
        output.discard(xsink);
        output = defaultValue();
        done = true;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (done) {
            return nullptr;
        }

        my_socket_priv* priv = my_socket_priv::getPriv(*sock);
        AutoLocker al(priv->m);
        if (priv->checkValid(xsink)) {
            output.discard(xsink);
            output = defaultValue();
            done = true;
            return nullptr;
        }

        qore_socket_private* sp = qore_socket_private::get(*priv->socket);
        switch (action) {
            case Action::GetCipherName:
                setStringValue(sp->ssl ? sp->ssl->getCipherName() : nullptr);
                break;

            case Action::GetCipherVersion:
                setStringValue(sp->ssl ? sp->ssl->getCipherVersion() : nullptr);
                break;

            case Action::IsSecure:
                output = static_cast<bool>(sp->ssl);
                break;

            case Action::SetAlpnProtocols:
                sp->alpn_protocols = protocols;
                output = 0;
                break;

            case Action::GetAlpnProtocol:
                setAlpnValue(sp);
                break;

            case Action::IsHttp2:
                output = sp->ssl ? sp->ssl->isHttp2() : false;
                break;

            case Action::VerifyPeerCertificate:
                output = sp->ssl ? static_cast<int64>(sp->ssl->verifyPeerCertificate()) : static_cast<int64>(-1);
                break;

            case Action::GetRemoteCertificate:
                if (sp->remote_cert) {
                    sp->remote_cert->ref();
                    output = sp->remote_cert;
                }
                break;
        }

        done = true;
        return nullptr;
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return output.refSelf();
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        return done ? "tls-state-done" : "querying-tls-state";
    }

private:
    DLLLOCAL QoreValue defaultValue() const {
        switch (action) {
            case Action::IsSecure:
            case Action::IsHttp2:
                return false;
            case Action::VerifyPeerCertificate:
                return static_cast<int64>(-1);
            case Action::SetAlpnProtocols:
                return static_cast<int64>(-1);
            case Action::GetCipherName:
            case Action::GetCipherVersion:
            case Action::GetAlpnProtocol:
            case Action::GetRemoteCertificate:
                return QoreValue();
        }
        return QoreValue();
    }

    DLLLOCAL void setStringValue(const char* str) {
        if (str && *str) {
            output = new QoreStringNode(str);
        }
    }

    DLLLOCAL void setAlpnValue(qore_socket_private* sp) {
        if (!sp->ssl) {
            return;
        }
        std::string proto = sp->ssl->getAlpnProtocol();
        if (!proto.empty()) {
            output = new QoreStringNode(proto.c_str());
        }
    }

    QoreSocketObject* sock;
    QoreValue output;
    Action action;
    std::vector<std::string> protocols;
    bool done = false;
};

static QoreObject* qore_socket_object_make_poll_op(QoreObject* sock_obj, SocketPollOperationBase* poller,
        const char* goal, ExceptionSink* xsink) {
    ReferenceHolder<SocketPollOperationBase> poller_holder(poller, xsink);
    if (*xsink) {
        return nullptr;
    }

    ReferenceHolder<QoreObject> op_obj(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), *poller_holder),
        xsink);
    poller_holder->setSelf(*op_obj);
    poller_holder.release();

    if (!*xsink) {
        op_obj->setValue("sock", sock_obj->objectRefSelf(), xsink);
        op_obj->setValue("goal", new QoreStringNode(goal), xsink);
    }
    return *xsink ? nullptr : op_obj.release();
}

static int qore_socket_object_exec_poll_no_output(QoreSocketObject* s, SocketPollOperationBase* poller,
        int timeout_ms, const char* owner_name, const char* goal, ExceptionSink* xsink) {
    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, poller, goal, xsink), xsink);
    if (*xsink) {
        return -1;
    }

    ReferenceHolder<QoreHashNode> result(
        qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj, timeout_ms, owner_name, xsink), xsink);
    return *xsink ? -1 : 0;
}

static QoreValue qore_socket_object_exec_poll_output(QoreSocketObject* s, SocketPollOperationBase* poller,
        int timeout_ms, const char* owner_name, const char* goal, ExceptionSink* xsink) {
    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, poller, goal, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }

    ReferenceHolder<QoreHashNode> result(
        qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj, timeout_ms, owner_name, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    return poller->getOutput();
}

static int qore_socket_object_exec_check_http1_allowed(QoreSocketObject* s, const char* mname,
        ExceptionSink* xsink) {
    ValueHolder rv(qore_socket_object_exec_poll_output(s,
        new QoreSocketObjectHttp1AllowedPollOperation(s, mname), -1, mname, "http1-checked", xsink), xsink);
    return *xsink ? -1 : 0;
}

static int32_t qore_socket_object_exec_get_http2_server_stream(QoreSocketObject* s, int32_t stream_id,
        const char* mname, ExceptionSink* xsink) {
    ValueHolder rv(qore_socket_object_exec_poll_output(s,
        new QoreSocketObjectHttp2ServerStreamPollOperation(s, stream_id, mname), -1, mname,
        "http2-server-stream-checked", xsink), xsink);
    if (*xsink) {
        return -1;
    }
    return rv->isNothing() ? -1 : static_cast<int32_t>(rv->getAsBigInt());
}

static QoreValue qore_socket_object_exec_http2_enqueue(QoreSocketObject* s,
        QoreSocketObjectHttp2EnqueuePollOperation* poller, const char* owner_name, ExceptionSink* xsink) {
    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, poller, "done", xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }

    ReferenceHolder<QoreHashNode> result(
        qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj, -1, owner_name, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    return poller->getOutput();
}

static int64 qore_socket_object_exec_http2_enqueue_int(QoreSocketObject* s,
        QoreSocketObjectHttp2EnqueuePollOperation* poller, const char* owner_name, ExceptionSink* xsink) {
    ValueHolder rv(qore_socket_object_exec_http2_enqueue(s, poller, owner_name, xsink), xsink);
    if (*xsink) {
        return -1;
    }
    return rv->isNothing() ? -1 : rv->getAsBigInt();
}

static BinaryNode* qore_socket_object_exec_http2_stream_data(QoreSocketObject* s,
        QoreSocketObjectHttp2StreamDataPollOperation* stream_data_poller, int32_t stream_id,
        ExceptionSink* xsink) {
    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, stream_data_poller, "http2-stream-data", xsink), xsink);
    if (*xsink) {
        return nullptr;
    }

    std::string owner_name("readHttp2StreamData:");
    owner_name += std::to_string(stream_id);
    ReferenceHolder<QoreHashNode> result(qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj,
        -1, owner_name.c_str(), xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    return stream_data_poller->takeOutput();
}

static bool qore_socket_object_exec_http2_stream_state(QoreSocketObject* s,
        QoreSocketObjectHttp2StreamStatePollOperation* stream_state_poller, int32_t stream_id,
        const char* owner_prefix, ExceptionSink* xsink) {
    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, stream_state_poller, "http2-stream-state", xsink), xsink);
    if (*xsink) {
        return true;
    }

    std::string owner_name(owner_prefix);
    owner_name += ':';
    owner_name += std::to_string(stream_id);
    ReferenceHolder<QoreHashNode> result(qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj,
        -1, owner_name.c_str(), xsink), xsink);
    if (*xsink) {
        return true;
    }

    QoreValue output = stream_state_poller->getOutput();
    return output.getAsBool();
}

static QoreValue qore_socket_object_exec_quic_enqueue(QoreSocketObject* s,
        QoreSocketObjectQuicEnqueuePollOperation* poller, const char* owner_name, ExceptionSink* xsink) {
    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, poller, "done", xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }

    ReferenceHolder<QoreHashNode> result(
        qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj, -1, owner_name, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    return poller->getOutput();
}

static int64 qore_socket_object_exec_quic_enqueue_int(QoreSocketObject* s,
        QoreSocketObjectQuicEnqueuePollOperation* poller, const char* owner_name, ExceptionSink* xsink) {
    ValueHolder rv(qore_socket_object_exec_quic_enqueue(s, poller, owner_name, xsink), xsink);
    if (*xsink) {
        return -1;
    }
    return rv->isNothing() ? -1 : rv->getAsBigInt();
}

static int qore_socket_object_exec_stream_drain(QoreSocketObject* s,
        QoreSocketObjectStreamDrainPollOperation* stream_drain_poller, int timeout_ms,
        const char* owner_name, ExceptionSink* xsink) {
    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, stream_drain_poller, "stream-drained", xsink), xsink);
    if (*xsink) {
        return -1;
    }

    QoreHashNode* ex = nullptr;
    ReferenceHolder<QoreHashNode> result(
        qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj, timeout_ms, owner_name, xsink, &ex), xsink);
    ReferenceHolder<QoreHashNode> ex_holder(ex, xsink);
    if (*xsink) {
        return -1;
    }
    if (ex_holder) {
        if (qore_socket_object_exec_exception_is(**ex_holder, "SOCKET-TIMEOUT")) {
            return 1;
        }
        qore_socket_object_raise_poll_result_exception(*ex_holder, xsink);
        return -1;
    }

    QoreValue output = stream_drain_poller->getOutput();
    return output.isNothing() ? -1 : static_cast<int>(output.getAsBigInt());
}

static BinaryNode* qore_socket_object_exec_quic_stream_data(QoreSocketObject* s,
        QoreSocketObjectQuicStreamDataPollOperation* stream_data_poller, int timeout_ms,
        int64_t session_id, int64_t stream_id, ExceptionSink* xsink) {
    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, stream_data_poller, "quic-stream-data", xsink), xsink);
    if (*xsink) {
        return nullptr;
    }

    QoreHashNode* ex = nullptr;
    std::string owner_name("readQuicStreamDataBlock:");
    owner_name += std::to_string(session_id);
    owner_name += ':';
    owner_name += std::to_string(stream_id);
    ReferenceHolder<QoreHashNode> result(qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj,
        timeout_ms, owner_name.c_str(), xsink, &ex), xsink);
    ReferenceHolder<QoreHashNode> ex_holder(ex, xsink);
    if (*xsink) {
        return nullptr;
    }
    if (ex_holder) {
        if (qore_socket_object_exec_exception_is(**ex_holder, "SOCKET-TIMEOUT")) {
            xsink->raiseException("QUIC-STREAM-TIMEOUT",
                "timeout reading QUIC stream data (session %lld, stream %lld)",
                (long long)session_id, (long long)stream_id);
            return nullptr;
        }
        qore_socket_object_raise_poll_result_exception(*ex_holder, xsink);
        return nullptr;
    }

    return stream_data_poller->takeOutput();
}

static QoreValue qore_socket_object_exec_quic_datagram(QoreSocketObject* s,
        QoreSocketObjectQuicDatagramPollOperation* datagram_poller, int timeout_ms, int64_t session_id,
        int64_t stream_id, ExceptionSink* xsink) {
    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, datagram_poller, "quic-datagram", xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }

    QoreHashNode* ex = nullptr;
    std::string owner_name("readQuicDatagram:");
    owner_name += std::to_string(session_id);
    owner_name += ':';
    owner_name += std::to_string(stream_id);
    ReferenceHolder<QoreHashNode> result(qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj,
        timeout_ms, owner_name.c_str(), xsink, &ex), xsink);
    ReferenceHolder<QoreHashNode> ex_holder(ex, xsink);
    if (*xsink) {
        return QoreValue();
    }
    if (ex_holder) {
        if (qore_socket_object_exec_exception_is(**ex_holder, "SOCKET-TIMEOUT")) {
            return QoreValue();
        }
        qore_socket_object_raise_poll_result_exception(*ex_holder, xsink);
        return QoreValue();
    }

    return datagram_poller->takeOutput();
}

static QoreValue qore_socket_object_exec_quic_connect_stream_data(QoreSocketObject* s,
        QoreSocketObjectQuicConnectStreamDataPollOperation* connect_data_poller, int64_t session_id,
        int64_t stream_id, ExceptionSink* xsink) {
    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, connect_data_poller, "quic-connect-stream-data", xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }

    std::string owner_name("readQuicConnectStreamData:");
    owner_name += std::to_string(session_id);
    owner_name += ':';
    owner_name += std::to_string(stream_id);
    ReferenceHolder<QoreHashNode> result(qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj,
        -1, owner_name.c_str(), xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    return connect_data_poller->takeOutput();
}

static QoreValue qore_socket_object_exec_quic_query(QoreSocketObject* s,
        QoreSocketObjectQuicQueryPollOperation* query_poller, const char* owner_name, ExceptionSink* xsink) {
    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, query_poller, "quic-query", xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }

    ReferenceHolder<QoreHashNode> result(
        qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj, -1, owner_name, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    return query_poller->getOutput();
}

class QoreSocketObjectAsyncIoGuard {
public:
    QoreSocketObjectAsyncIoGuard(my_socket_priv& priv, ExceptionSink* xsink, unsigned direction)
            : priv(priv), direction(direction) {
        SocketSyncPoll::assertNotOnIoThread("Socket", "sync", xsink);
        if (qore_on_async_io_thread() || (xsink && *xsink)) {
            return;
        }
        AutoLocker al(priv.m);
        active = !priv.startAsyncSequenceIo(xsink, direction);
    }

    ~QoreSocketObjectAsyncIoGuard() {
        if (active) {
            AutoLocker al(priv.m);
            priv.clearAsyncSequenceIo(direction);
        }
    }

    explicit operator bool() const {
        return active;
    }

private:
    my_socket_priv& priv;
    unsigned direction;
    bool active = false;
};

static int qore_socket_object_exec_connect(QoreSocketObject* s, const char* target, int timeout_ms, bool ssl,
        ExceptionSink* xsink) {
    s->ref();
    ReferenceHolder<SocketPollOperationBase> poller(new SocketConnectPollOperation(xsink, ssl, target, s, true),
        xsink);
    if (*xsink) {
        return -1;
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_ALL);
    if (!async_guard) {
        return -1;
    }

    const char* goal = ssl ? "connect-ssl" : "connect";
    return qore_socket_object_exec_poll_no_output(s, poller.release(), timeout_ms, goal, goal, xsink);
}

static int qore_socket_object_exec_connect_inet(QoreSocketObject* s, const char* host, const char* service,
        int family, int socktype, int protocol, int timeout_ms, bool ssl, ExceptionSink* xsink) {
    s->ref();
    ReferenceHolder<SocketPollOperationBase> poller(
        new SocketConnectPollOperation(xsink, ssl, host, service, family, socktype, protocol, s, true), xsink);
    if (*xsink) {
        return -1;
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_ALL);
    if (!async_guard) {
        return -1;
    }

    const char* goal = ssl ? "connect-ssl" : "connect";
    return qore_socket_object_exec_poll_no_output(s, poller.release(), timeout_ms, goal, goal, xsink);
}

static int qore_socket_object_exec_connect_unix(QoreSocketObject* s, const char* path, int socktype, int protocol,
        int timeout_ms, bool ssl, ExceptionSink* xsink) {
    s->ref();
    ReferenceHolder<SocketPollOperationBase> poller(
        new SocketConnectPollOperation(xsink, ssl, path, socktype, protocol, s, true), xsink);
    if (*xsink) {
        return -1;
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_ALL);
    if (!async_guard) {
        return -1;
    }

    const char* goal = ssl ? "connect-ssl" : "connect";
    return qore_socket_object_exec_poll_no_output(s, poller.release(), timeout_ms, goal, goal, xsink);
}

static int qore_socket_object_exec_upgrade_ssl(QoreSocketObject* s, int timeout_ms, bool server,
        ExceptionSink* xsink) {
    s->ref();
    ReferenceHolder<SocketPollOperationBase> poller(
        server
            ? static_cast<SocketPollOperationBase*>(new SocketUpgradeServerSslPollOperation(xsink, s, true))
            : static_cast<SocketPollOperationBase*>(new SocketUpgradeClientSslPollOperation(xsink, s, true)),
        xsink);
    if (*xsink) {
        return -1;
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_ALL);
    if (!async_guard) {
        return -1;
    }

    const char* goal = server ? "upgrade-server-ssl" : "upgrade-client-ssl";
    return qore_socket_object_exec_poll_no_output(s, poller.release(), timeout_ms, goal, goal, xsink);
}

static int qore_socket_object_exec_shutdown_ssl(QoreSocketObject* s, ExceptionSink* xsink) {
    s->ref();
    ReferenceHolder<SocketPollOperationBase> poller(new SocketShutdownSslPollOperation(xsink, s, true), xsink);
    if (*xsink) {
        return -1;
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_ALL);
    if (!async_guard) {
        return -1;
    }

    return qore_socket_object_exec_poll_no_output(s, poller.release(), -1, "shutdownSSL", "shutdown-ssl", xsink);
}

static int qore_socket_object_exec_http2_flush(QoreSocketObject* s, const char* owner_name, ExceptionSink* xsink,
        bool submit_ping = false, bool missing_h2_ok = false) {
    s->ref();
    ReferenceHolder<SocketPollOperationBase> poller(
        new SocketHttp2FlushPollOperation(xsink, s, true, submit_ping, missing_h2_ok), xsink);
    if (*xsink) {
        return -1;
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_ALL);
    if (!async_guard) {
        return -1;
    }

    return qore_socket_object_exec_poll_no_output(s, poller.release(), -1, owner_name, "done", xsink);
}

static int qore_socket_object_exec_shutdown(QoreSocketObject* s, ExceptionSink* xsink) {
    s->ref();
    SocketShutdownPollOperation* shutdown_poller = new SocketShutdownPollOperation(s);
    ReferenceHolder<SocketPollOperationBase> poller(shutdown_poller, xsink);
    if (*xsink) {
        return -1;
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_ALL);
    if (!async_guard) {
        return -1;
    }

    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, poller.release(), "shutdown", xsink), xsink);
    if (*xsink) {
        return -1;
    }

    ReferenceHolder<QoreHashNode> result(
        qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj, -1, "shutdown", xsink), xsink);
    return *xsink ? -1 : shutdown_poller->getRc();
}

static int qore_socket_object_exec_close(QoreSocketObject* s) {
    ExceptionSink xsink;

    ReferenceHolder<QoreObject> ctl_obj(qore_get_async_io_controller_obj(&xsink), &xsink);
    if (!ctl_obj) {
        return -1;
    }
    ReferenceHolder<AsyncIoControllerPriv> ctrl(
        static_cast<AsyncIoControllerPriv*>(
            (*ctl_obj)->getReferencedPrivateData(CID_ASYNCIOCONTROLLER, &xsink)), &xsink);
    if (xsink || !ctrl) {
        xsink.clear();
        return -1;
    }

    int rc = ctrl->close(s, &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

static int qore_socket_object_exec_setup(QoreSocketObject* s, SocketSetupPollOperation* setup_poller,
        const char* owner_name, const char* goal, ExceptionSink* xsink) {
    s->ref();
    ReferenceHolder<SocketSetupPollOperation> poller(setup_poller, xsink);
    if (*xsink) {
        return -1;
    }

    bool config_action = poller->isConfigAction();
    if (!config_action) {
        my_socket_priv* priv = my_socket_priv::getPriv(*s);
        QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_ALL);
        if (!async_guard) {
            return -1;
        }

        ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
        ReferenceHolder<QoreObject> op_obj(
            qore_socket_object_make_poll_op(*sock_obj, poller.release(), goal, xsink), xsink);
        if (*xsink) {
            return -1;
        }

        ReferenceHolder<QoreHashNode> result(
            qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj, -1, owner_name, xsink), xsink);
        return *xsink ? -1 : setup_poller->getRc();
    }

    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, poller.release(), goal, xsink), xsink);
    if (*xsink) {
        return -1;
    }

    ReferenceHolder<QoreHashNode> result(
        qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj, -1, owner_name, xsink), xsink);
    return *xsink ? -1 : setup_poller->getRc();
}

static QoreHashNode* qore_socket_object_get_addr_info_from_output(const QoreValue output, const char* err,
        ExceptionSink* xsink) {
    if (output.getType() != NT_HASH) {
        xsink->raiseException(err, "expected address information from async socket operation, got '%s'",
            output.getFullTypeName());
        return nullptr;
    }

    const QoreHashNode* h = output.get<const QoreHashNode>();
    QoreValue addr_value = h->getKeyValue("addr");
    QoreValue len_value = h->getKeyValue("len");
    if (addr_value.getType() != NT_BINARY || len_value.getType() != NT_INT) {
        xsink->raiseException(err, "invalid address information from async socket operation");
        return nullptr;
    }

    const BinaryNode* bin = addr_value.get<const BinaryNode>();
    if (bin->size() != sizeof(struct sockaddr_storage)) {
        xsink->raiseException(err, "invalid socket address size from async socket operation: %zu", bin->size());
        return nullptr;
    }

    int64 raw_len = len_value.getAsBigInt();
    if (raw_len <= 0 || raw_len > static_cast<int64>(sizeof(struct sockaddr_storage))) {
        xsink->raiseException(err, "invalid socket address length from async socket operation: " QLLD, raw_len);
        return nullptr;
    }

    struct sockaddr_storage addr = {};
    memcpy(&addr, bin->getPtr(), sizeof(addr));

    std::string socketname;
    QoreValue socketname_value = h->getKeyValue("socketname");
    if (socketname_value.getType() == NT_STRING) {
        socketname = socketname_value.get<const QoreStringNode>()->c_str();
    }

    std::string hostname;
    QoreValue hostname_value = h->getKeyValue("hostname");
    if (hostname_value.getType() == NT_STRING) {
        hostname = hostname_value.get<const QoreStringNode>()->c_str();
    }

    return qore_socket_private::makeAddrInfo(addr, static_cast<socklen_t>(raw_len), socketname,
        hostname.empty() ? nullptr : hostname.c_str());
}

static QoreHashNode* qore_socket_object_exec_address_info(QoreSocketObject* s,
        QoreSocketObjectAddressInfoPollOperation::Action action, bool host_lookup, const char* owner_name,
        const char* err, ExceptionSink* xsink) {
    s->ref();
    QoreSocketObjectAddressInfoPollOperation* poller = new QoreSocketObjectAddressInfoPollOperation(s, action,
        host_lookup);

    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, poller, "done", xsink), xsink);
    if (*xsink) {
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> result(
        qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj, -1, owner_name, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }

    ValueHolder output(poller->getOutput(), xsink);
    return *xsink ? nullptr : qore_socket_object_get_addr_info_from_output(*output, err, xsink);
}

void my_socket_priv::setAccept(QoreSocketObject& sock, QoreObject* o) {
    ExceptionSink xsink;
    ReferenceHolder<QoreHashNode> info(qore_socket_object_exec_address_info(&sock,
        QoreSocketObjectAddressInfoPollOperation::Action::Peer, true, "setAccept",
        "SOCKET-GETPEERINFO-ERROR", &xsink), &xsink);
    if (!xsink && info) {
        qore_socket_private::setAccept(o, **info);
    }
    if (xsink) {
        xsink.clear();
    }
}

static QoreSocketObject* qore_socket_object_exec_accept(QoreSocketObject* s, int timeout_ms, bool ssl,
        ExceptionSink* xsink, SocketSource* source = nullptr) {
    s->ref();
    SocketAcceptPollOperation* accept_poller = new SocketAcceptPollOperation(xsink, s, ssl, source, true);
    ReferenceHolder<SocketPollOperationBase> poller(accept_poller, xsink);
    if (*xsink) {
        return nullptr;
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_ALL);
    if (!async_guard) {
        return nullptr;
    }

    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    const char* goal = ssl ? "accept-ssl" : "accept";
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, poller.release(), goal, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }

    QoreHashNode* ex = nullptr;
    ReferenceHolder<QoreHashNode> result(
        qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj, timeout_ms, goal, xsink, &ex), xsink);
    ReferenceHolder<QoreHashNode> ex_holder(ex, xsink);
    if (*xsink) {
        return nullptr;
    }
    if (ex_holder) {
        if (qore_socket_object_exec_exception_is(**ex_holder, "SOCKET-TIMEOUT")
                && !strcmp(static_cast<SocketPollOperationBase*>(accept_poller)->getStateImpl(), "accepting")) {
            return nullptr;
        }
        qore_socket_object_raise_poll_result_exception(*ex_holder, xsink);
        return nullptr;
    }

    ValueHolder output(accept_poller->getOutput(), xsink);
    if (*xsink || output->isNothing()) {
        return nullptr;
    }
    if (output->getType() != NT_OBJECT) {
        xsink->raiseException("SOCKET-ACCEPT-ERROR",
            "expected Socket object from async accept operation, got '%s'",
            output->getFullTypeName());
        return nullptr;
    }

    QoreObject* ns = output->get<QoreObject>();
    QoreSocketObject* accepted = static_cast<QoreSocketObject*>(ns->getReferencedPrivateData(CID_SOCKET, xsink));
    if (*xsink) {
        return nullptr;
    }
    if (!accepted) {
        xsink->raiseException("SOCKET-ACCEPT-ERROR", "accepted object does not contain Socket private data");
    }
    return accepted;
}

static int qore_socket_object_exec_send_poll(QoreSocketObject* s, SocketPollOperationBase* poller,
        int timeout_ms, ExceptionSink* xsink, unsigned async_direction = NB_SEND) {
    ReferenceHolder<SocketPollOperationBase> poller_holder(poller, xsink);
    if (*xsink) {
        return -1;
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, async_direction);
    if (!async_guard) {
        return -1;
    }

    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, poller_holder.release(), "send", xsink), xsink);
    if (*xsink) {
        return -1;
    }

    ReferenceHolder<QoreHashNode> result(
        qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj, timeout_ms, "send", xsink), xsink);
    return *xsink ? -1 : 0;
}

static int qore_socket_object_exec_send_binary(QoreSocketObject* s, BinaryNode* data,
        int timeout_ms, ExceptionSink* xsink) {
    s->ref();
    return qore_socket_object_exec_send_poll(s, new SocketSendPollOperation(xsink, data, s, true), timeout_ms, xsink);
}

static int qore_socket_object_exec_send_bytes(QoreSocketObject* s, const void* data, size_t size,
        int timeout_ms, ExceptionSink* xsink) {
    SimpleRefHolder<BinaryNode> bin(new BinaryNode());
    bin->append(data, size);
    return qore_socket_object_exec_send_binary(s, bin.release(), timeout_ms, xsink);
}

struct SocketObjectInputStreamRefGuard {
    SocketObjectInputStreamRefGuard(InputStream* is, ExceptionSink* xsink, bool active)
            : is(active ? is : nullptr), xsink(xsink) {
        if (this->is) {
            this->is->ref();
        }
    }

    ~SocketObjectInputStreamRefGuard() {
        if (is) {
            is->deref(xsink);
        }
    }

private:
    InputStream* is;
    ExceptionSink* xsink;
};

struct SocketObjectOutputStreamRefGuard {
    SocketObjectOutputStreamRefGuard(OutputStream* os, ExceptionSink* xsink, bool active)
            : os(active ? os : nullptr), xsink(xsink) {
        if (this->os) {
            this->os->ref();
        }
    }

    ~SocketObjectOutputStreamRefGuard() {
        if (os) {
            os->deref(xsink);
        }
    }

private:
    OutputStream* os;
    ExceptionSink* xsink;
};

static int qore_socket_object_exec_send_input_stream_poll(QoreSocketObject* s, InputStream* is,
        QoreObject* is_obj, int64 size, int timeout_ms, ExceptionSink* xsink, bool reassign_after) {
    if (!size) {
        return 0;
    }
    if (!is->isIoThreadSafe()) {
        xsink->raiseException("SOCKET-SEND-ERROR", "InputStream is not I/O thread safe");
        return -1;
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_SEND);
    if (!async_guard) {
        return -1;
    }

    SocketObjectInputStreamRefGuard caller_ref(is, xsink, reassign_after);
    s->ref();
    is->ref();
    if (is_obj) {
        is_obj->ref();
    }
    ReferenceHolder<SocketPollOperationBase> poller(
        new SocketSendInputStreamPollOperation(xsink, s, is, is_obj, size, timeout_ms, true, true), xsink);
    if (*xsink) {
        return -1;
    }

    is->unassignThread(xsink);
    if (*xsink) {
        return -1;
    }

    int rc = qore_socket_object_exec_send_poll(s, poller.release(), -1, xsink);
    if (reassign_after) {
        if (!*xsink) {
            is->reassignThread(xsink);
        } else {
            ExceptionSink reassign_xsink;
            is->reassignThread(&reassign_xsink);
            if (reassign_xsink) {
                reassign_xsink.clear();
            }
        }
    }
    return rc;
}

static int qore_socket_object_exec_send_http_chunked_body_input_stream_poll(QoreSocketObject* s,
        InputStream* is, QoreObject* is_obj, size_t max_chunk_size, int timeout_ms, bool send_terminal_chunk,
        ExceptionSink* xsink, bool reassign_after) {
    if (!is->isIoThreadSafe()) {
        xsink->raiseException("SOCKET-SEND-ERROR", "InputStream is not I/O thread safe");
        return -1;
    }

    SocketObjectInputStreamRefGuard caller_ref(is, xsink, reassign_after);
    s->ref();
    is->ref();
    if (is_obj) {
        is_obj->ref();
    }
    ReferenceHolder<SocketPollOperationBase> poller(
        new SocketSendHttpChunkedInputStreamPollOperation(xsink, s, is, is_obj, max_chunk_size, timeout_ms,
            send_terminal_chunk, QORE_SOURCE_SOCKET, true),
        xsink);
    if (*xsink) {
        return -1;
    }

    is->unassignThread(xsink);
    if (*xsink) {
        return -1;
    }

    int rc = qore_socket_object_exec_send_poll(s, poller.release(), -1, xsink);
    if (reassign_after) {
        if (!*xsink) {
            is->reassignThread(xsink);
        } else {
            ExceptionSink reassign_xsink;
            is->reassignThread(&reassign_xsink);
            if (reassign_xsink) {
                reassign_xsink.clear();
            }
        }
    }
    return rc;
}

static int qore_socket_object_exec_recv_output_stream_poll(QoreSocketObject* s, OutputStream* os,
        QoreObject* os_obj, int64 size, int timeout_ms, ExceptionSink* xsink, bool reassign_after,
        bool emit_data_events) {
    if (!size) {
        return 0;
    }
    if (!os->isIoThreadSafe()) {
        xsink->raiseException("SOCKET-RECV-ERROR", "OutputStream is not I/O thread safe");
        return -1;
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_RECV);
    if (!async_guard) {
        return -1;
    }

    SocketObjectOutputStreamRefGuard caller_ref(os, xsink, reassign_after);
    s->ref();
    os->ref();
    if (os_obj) {
        os_obj->ref();
    }
    ReferenceHolder<SocketPollOperationBase> poller(
        new SocketRecvOutputStreamPollOperation(xsink, s, os, os_obj, size, timeout_ms, emit_data_events, true),
        xsink);
    if (*xsink) {
        return -1;
    }

    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, poller.release(), "received", xsink), xsink);
    if (*xsink) {
        return -1;
    }

    os->unassignThread(xsink);
    if (*xsink) {
        return -1;
    }

    ReferenceHolder<QoreHashNode> result(
        qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj, -1, "recvToOutputStream", xsink), xsink);
    if (reassign_after) {
        if (!*xsink) {
            os->reassignThread(xsink);
        } else {
            ExceptionSink reassign_xsink;
            os->reassignThread(&reassign_xsink);
            if (reassign_xsink) {
                reassign_xsink.clear();
            }
        }
    }
    return *xsink ? -1 : 0;
}

static int qore_socket_object_exec_write_output_stream_poll(QoreSocketObject* s, OutputStream* os,
        QoreObject* os_obj, const BinaryNode& data, int timeout_ms, ExceptionSink* xsink, bool reassign_after) {
    if (!data.size()) {
        return 0;
    }
    if (!os->isIoThreadSafe()) {
        xsink->raiseException("SOCKET-WRITE-ERROR", "OutputStream is not I/O thread safe");
        return -1;
    }

    SocketObjectOutputStreamRefGuard caller_ref(os, xsink, reassign_after);
    s->ref();
    os->ref();
    if (os_obj) {
        os_obj->ref();
    }
    ReferenceHolder<SocketPollOperationBase> poller(
        new SocketWriteOutputStreamPollOperation(xsink, s, os, os_obj,
            static_cast<BinaryNode*>(data.refSelf()), timeout_ms), xsink);
    if (*xsink) {
        return -1;
    }

    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, poller.release(), "written", xsink), xsink);
    if (*xsink) {
        return -1;
    }

    os->unassignThread(xsink);
    if (*xsink) {
        return -1;
    }

    ReferenceHolder<QoreHashNode> result(
        qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj, -1, "writeOutputStream", xsink), xsink);
    if (reassign_after) {
        if (!*xsink) {
            os->reassignThread(xsink);
        } else {
            ExceptionSink reassign_xsink;
            os->reassignThread(&reassign_xsink);
            if (reassign_xsink) {
                reassign_xsink.clear();
            }
        }
    }
    return *xsink ? -1 : 0;
}

static int qore_socket_object_exec_send_string(QoreSocketObject* s, const QoreStringNode& data,
        int timeout_ms, ExceptionSink* xsink, QoreStringNode** sent_data = nullptr) {
    SimpleRefHolder<QoreStringNode> tmp;
    if (data.getEncoding() != s->getEncoding()) {
        tmp = data.convertEncoding(s->getEncoding(), xsink);
        if (*xsink) {
            return -1;
        }
    }

    if (sent_data) {
        *sent_data = tmp ? tmp->stringRefSelf() : data.stringRefSelf();
    }

    s->ref();
    return qore_socket_object_exec_send_poll(s,
        new SocketSendPollOperation(xsink, tmp ? tmp.release() : data.stringRefSelf(), s, true), timeout_ms, xsink);
}

static QoreValue qore_socket_object_exec_recv_poll(QoreSocketObject* s, SocketRecvPollOperationBase* poller,
        int timeout_ms, const char* owner_name, ExceptionSink* xsink) {
    SocketRecvPollOperationBase* recv_poller = poller;
    ReferenceHolder<SocketPollOperationBase> poller_holder(poller, xsink);
    if (*xsink) {
        return QoreValue();
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_RECV);
    if (!async_guard) {
        return QoreValue();
    }

    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, poller_holder.release(), "received", xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }

    ReferenceHolder<QoreHashNode> result(
        qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj, timeout_ms, owner_name, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    return recv_poller->getOutput();
}

static QoreValue qore_socket_object_exec_recv_value(QoreSocketObject* s, int64 size, int timeout_ms,
        bool to_string, ExceptionSink* xsink) {
    s->ref();
    if (size > 0) {
        if (size > std::numeric_limits<ssize_t>::max()) {
            xsink->raiseException("SOCKET-RECV-ERROR",
                "requested receive size " QLLD " exceeds the maximum supported size",
                size);
            s->deref(xsink);
            return QoreValue();
        }
        return qore_socket_object_exec_recv_poll(s,
            new SocketRecvPollOperation(xsink, static_cast<ssize_t>(size), s, to_string, true),
            timeout_ms, "recv", xsink);
    }
    return qore_socket_object_exec_recv_poll(s, new SocketRecvDataPollOperation(xsink, s, to_string, true),
        timeout_ms, "recv", xsink);
}

static QoreStringNode* qore_socket_object_exec_recv_string(QoreSocketObject* s, int64 size,
        int timeout_ms, ExceptionSink* xsink) {
    ValueHolder result(qore_socket_object_exec_recv_value(s, size, timeout_ms, true, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    if (result->isNothing()) {
        return new QoreStringNode;
    }
    if (result->getType() != NT_STRING) {
        xsink->raiseException("SOCKET-RECV-ERROR", "expected string data from async receive operation, got '%s'",
            result->getFullTypeName());
        return nullptr;
    }
    return result.release().get<QoreStringNode>();
}

static BinaryNode* qore_socket_object_exec_recv_binary(QoreSocketObject* s, int64 size,
        int timeout_ms, ExceptionSink* xsink) {
    ValueHolder result(qore_socket_object_exec_recv_value(s, size, timeout_ms, false, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    if (result->isNothing()) {
        return new BinaryNode;
    }
    if (result->getType() != NT_BINARY) {
        xsink->raiseException("SOCKET-RECV-ERROR", "expected binary data from async receive operation, got '%s'",
            result->getFullTypeName());
        return nullptr;
    }
    return result.release().get<BinaryNode>();
}

static BinaryNode* qore_socket_object_exec_recv_bytes(QoreSocketObject* s, size_t size,
        int timeout_ms, ExceptionSink* xsink) {
    SimpleRefHolder<BinaryNode> bin(qore_socket_object_exec_recv_binary(s, size, timeout_ms, xsink));
    if (*xsink) {
        return nullptr;
    }
    if (bin->size() != size) {
        xsink->raiseException("SOCKET-RECV-ERROR",
            "expected " QLLD " byte(s) from async receive operation, got " QLLD,
            static_cast<int64>(size), static_cast<int64>(bin->size()));
        return nullptr;
    }
    return bin.release();
}

static BinaryNode* qore_socket_object_exec_recv_some_binary(QoreSocketObject* s, size_t size,
        int timeout_ms, ExceptionSink* xsink, const char* owner_name = "recvToOutputStream") {
    s->ref();
    ValueHolder result(qore_socket_object_exec_recv_poll(s,
        new SocketRecvSomePollOperation(xsink, static_cast<ssize_t>(size), s, false, true),
        timeout_ms, owner_name, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    if (result->isNothing()) {
        return new BinaryNode;
    }
    if (result->getType() != NT_BINARY) {
        xsink->raiseException("SOCKET-RECV-ERROR", "expected binary data from async receive operation, got '%s'",
            result->getFullTypeName());
        return nullptr;
    }
    return result.release().get<BinaryNode>();
}

static QoreHashNode* qore_socket_object_exec_read_http_header(QoreSocketObject* s, QoreHashNode* info,
        int timeout_ms, ExceptionSink* xsink) {
    s->ref();
    SocketReadHttpHeaderPollOperation* header_poller = new SocketReadHttpHeaderPollOperation(xsink, s, true);
    ReferenceHolder<SocketPollOperationBase> poller(header_poller, xsink);
    if (*xsink) {
        return nullptr;
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_RECV);
    if (!async_guard) {
        return nullptr;
    }

    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, poller.release(), "received", xsink), xsink);
    if (*xsink) {
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> result(
        qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj, timeout_ms, "readHTTPHeader", xsink), xsink);
    if (*xsink) {
        return nullptr;
    }

    ValueHolder output(header_poller->getOutput(), xsink);
    if (*xsink) {
        return nullptr;
    }
    if (output->getType() != NT_HASH) {
        xsink->raiseException("SOCKET-HTTP-ERROR",
            "expected hash output from async HTTP header operation, got '%s'",
            output->getFullTypeName());
        return nullptr;
    }

    QoreHashNode* output_hash = output->get<QoreHashNode>();
    QoreValue hdr_val = output_hash->getKeyValue("hdr");
    if (hdr_val.getType() != NT_HASH) {
        xsink->raiseException("SOCKET-HTTP-ERROR",
            "expected 'hdr' hash output from async HTTP header operation, got '%s'",
            hdr_val.getFullTypeName());
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> hdr(hdr_val.get<const QoreHashNode>()->hashRefSelf(), xsink);
    if (info) {
        QoreValue info_val = output_hash->getKeyValue("info");
        if (info_val.getType() != NT_HASH) {
            xsink->raiseException("SOCKET-HTTP-ERROR",
                "expected 'info' hash output from async HTTP header operation, got '%s'",
                info_val.getFullTypeName());
            return nullptr;
        }
        info->merge(info_val.get<const QoreHashNode>(), xsink);
    }
    return *xsink ? nullptr : hdr.release();
}

static QoreStringNode* qore_socket_object_exec_read_http_header_string(QoreSocketObject* s,
        int timeout_ms, ExceptionSink* xsink) {
    SimpleRefHolder<QoreStringNode> pattern(new QoreStringNode("\r\n\r\n"));
    s->ref();
    ValueHolder result(qore_socket_object_exec_recv_poll(s,
        new SocketRecvUntilBytesPollOperation(xsink, *pattern, s, true, true),
        timeout_ms, "readHTTPHeaderString", xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    if (result->getType() != NT_STRING) {
        xsink->raiseException("SOCKET-HTTP-ERROR",
            "expected string output from async HTTP header string operation, got '%s'",
            result->getFullTypeName());
        return nullptr;
    }

    QoreStringNode* raw = result->get<QoreStringNode>();
    const size_t len = raw->size();
    SimpleRefHolder<QoreStringNode> hdr;
    if (len >= 4 && !memcmp(raw->c_str() + len - 4, "\r\n\r\n", 4)) {
        hdr = new QoreStringNode(raw->c_str(), len - 4, raw->getEncoding());
        hdr->concat('\n');
    } else {
        hdr = raw->stringRefSelf();
    }

    my_socket_priv::getPriv(*s)->doDataEvent(QORE_EVENT_HTTP_HEADERS_READ, QORE_SOURCE_SOCKET, **hdr);
    return hdr.release();
}

static size_t qore_socket_object_exec_http_line_payload_size(const QoreStringNode& line) {
    size_t len = line.size();
    while (len && (line.c_str()[len - 1] == '\r' || line.c_str()[len - 1] == '\n')) {
        --len;
    }
    return len;
}

static bool qore_socket_object_exec_http_blank_line(const QoreStringNode& line) {
    return !qore_socket_object_exec_http_line_payload_size(line);
}

static int qore_socket_object_exec_run_http_recv_data_callback(
        const ResolvedCallReferenceNode* recv_callback, const AbstractQoreNode& data, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
    ReferenceHolder<QoreHashNode> arg(new QoreHashNode(autoTypeInfo), xsink);
    arg->setKeyValue("data", data.realCopy(), xsink);
    arg->setKeyValue("chunked", true, xsink);
    args->push(arg.release(), nullptr);
    if (*xsink) {
        return -1;
    }

    ValueHolder rv(recv_callback->execValue(*args, xsink), xsink);
    return *xsink ? -1 : 0;
}

static int qore_socket_object_exec_run_http_recv_header_callback(QoreObject* obj,
        const ResolvedCallReferenceNode* recv_callback, const QoreHashNode* hdr, QoreHashNode* info,
        ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
    ReferenceHolder<QoreHashNode> arg(new QoreHashNode(autoTypeInfo), xsink);
    arg->setKeyValue("hdr", hdr ? hdr->refSelf() : nullptr, xsink);
    arg->setKeyValue("info", info, xsink);
    if (obj) {
        arg->setKeyValue("obj", obj->objectRefSelf(), xsink);
    }
    arg->setKeyValue("send_aborted", false, xsink);
    args->push(arg.release(), nullptr);
    if (*xsink) {
        return -1;
    }

    ValueHolder rv(recv_callback->execValue(*args, xsink), xsink);
    return *xsink ? -1 : 0;
}

static QoreStringNode* qore_socket_object_exec_recv_http_line(QoreSocketObject* s, int timeout_ms,
        const char* owner_name, ExceptionSink* xsink) {
    SimpleRefHolder<QoreStringNode> pattern(new QoreStringNode("\r\n"));
    s->ref();
    ValueHolder result(qore_socket_object_exec_recv_poll(s,
        new SocketRecvUntilBytesPollOperation(xsink, *pattern, s, true, true),
        timeout_ms, owner_name, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    if (result->getType() != NT_STRING) {
        xsink->raiseException("SOCKET-RECV-ERROR", "expected string data from async receive operation, got '%s'",
            result->getFullTypeName());
        return nullptr;
    }
    return result.release().get<QoreStringNode>();
}

static int qore_socket_object_exec_read_http_chunked_trailers(QoreSocketObject* s, QoreHashNode& output,
        int timeout_ms, const char* owner_name, ExceptionSink* xsink, QoreHashNode* info = nullptr,
        bool* has_trailers = nullptr) {
    QoreString trailers(s->getEncoding());
    unsigned cancel_check = 0;
    while (true) {
        if (!(cancel_check++ % 100) && qore_check_cancel(xsink, "socket chunked trailer read")) {
            return -1;
        }

        SimpleRefHolder<QoreStringNode> line(
            qore_socket_object_exec_recv_http_line(s, timeout_ms, owner_name, xsink));
        if (*xsink) {
            return -1;
        }
        if (qore_socket_object_exec_http_blank_line(**line)) {
            break;
        }
        trailers.concat(line->c_str(), line->size());
    }

    if (has_trailers) {
        *has_trailers = !trailers.empty();
    }
    if (!trailers.empty()) {
        my_socket_priv* priv = my_socket_priv::getPriv(*s);
        priv->convertHeaderToHash(output, trailers, info);
        priv->doReadHttpHeaderEvent(QORE_EVENT_HTTP_FOOTERS_RECEIVED, output, QORE_SOURCE_SOCKET);
    }
    return 0;
}

static QoreHashNode* qore_socket_object_exec_read_http_chunked_body(QoreSocketObject* s, int timeout_ms,
        bool binary_body, bool read_once, const char* owner_name, ExceptionSink* xsink, OutputStream* os = nullptr,
        const ResolvedCallReferenceNode* recv_callback = nullptr, QoreObject* obj = nullptr) {
    assert(!os || (binary_body && !read_once && !recv_callback));
    assert(!recv_callback || !read_once);

    SocketSyncPoll::assertNotOnIoThread("Socket", owner_name, xsink);
    if (*xsink) {
        return nullptr;
    }

    if (!os && !recv_callback) {
        s->ref();
        ValueHolder rv(qore_socket_object_exec_recv_poll(s,
            new SocketReadHttpChunkedBodyPollOperation(xsink, s, binary_body, read_once, QORE_SOURCE_SOCKET),
            timeout_ms, owner_name, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        if (rv->getType() != NT_HASH) {
            xsink->raiseException("READ-HTTP-CHUNK-ERROR",
                "expected hash output from async chunked body read operation, got '%s'", rv->getFullTypeName());
            return nullptr;
        }
        return rv.release().get<QoreHashNode>();
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_RECV);
    if (!async_guard) {
        return nullptr;
    }

    priv->clearHttpExpectChunkedBody();

    SimpleRefHolder<BinaryNode> body_bin(binary_body && !os && !recv_callback ? new BinaryNode : nullptr);
    SimpleRefHolder<QoreStringNode> body_str(binary_body || recv_callback
        ? nullptr
        : new QoreStringNode(s->getEncoding()));

    unsigned cancel_check = 0;
    while (true) {
        if (!(cancel_check++ % 100) && qore_check_cancel(xsink, "socket chunked body read")) {
            return nullptr;
        }

        SimpleRefHolder<QoreStringNode> line(
            qore_socket_object_exec_recv_http_line(s, timeout_ms, owner_name, xsink));
        if (*xsink) {
            return nullptr;
        }

        size_t line_len = qore_socket_object_exec_http_line_payload_size(**line);
        const char* line_str = line->c_str();
        const char* semi = static_cast<const char*>(memchr(line_str, ';', line_len));
        size_t hex_len = semi ? static_cast<size_t>(semi - line_str) : line_len;
        std::string hex(line_str, hex_len);
        long chunk_size = strtol(hex.c_str(), nullptr, 16);
        if (chunk_size < 0) {
            xsink->raiseException("READ-HTTP-CHUNK-ERROR", "negative value given for chunk size (%ld)", chunk_size);
            return nullptr;
        }

        priv->doChunkedReadEvent(QORE_EVENT_HTTP_CHUNK_SIZE, static_cast<size_t>(chunk_size), line_len,
            QORE_SOURCE_SOCKET);

        if (!chunk_size) {
            ReferenceHolder<QoreHashNode> output(new QoreHashNode(autoTypeInfo), xsink);
            if (!os && !recv_callback) {
                if (binary_body) {
                    output->setKeyValue("body", body_bin.release(), xsink);
                } else {
                    output->setKeyValue("body", body_str.release(), xsink);
                }
            }
            ReferenceHolder<QoreHashNode> info(recv_callback ? new QoreHashNode(autoTypeInfo) : nullptr, xsink);
            bool has_trailers = false;
            if (*xsink || qore_socket_object_exec_read_http_chunked_trailers(s, **output, timeout_ms, owner_name,
                    xsink, *info, &has_trailers)) {
                return nullptr;
            }
            if (recv_callback) {
                if (qore_socket_object_exec_run_http_recv_header_callback(obj, recv_callback,
                        has_trailers ? *output : nullptr, info.release(), xsink)) {
                    return nullptr;
                }
                return nullptr;
            }
            return output.release();
        }

        if (static_cast<unsigned long>(chunk_size) > static_cast<unsigned long>(std::numeric_limits<ssize_t>::max())) {
            xsink->raiseException("READ-HTTP-CHUNK-ERROR", "chunk size %ld exceeds the maximum supported size",
                chunk_size);
            return nullptr;
        }

        SimpleRefHolder<BinaryNode> chunk(qore_socket_object_exec_recv_bytes(s, static_cast<size_t>(chunk_size),
            timeout_ms, xsink));
        if (*xsink) {
            return nullptr;
        }

        priv->doDataEvent(QORE_EVENT_HTTP_CHUNKED_DATA_READ, QORE_SOURCE_SOCKET, chunk->getPtr(), chunk->size());
        if (os) {
            int write_rc = qore_socket_object_exec_write_output_stream_poll(s, os, nullptr, **chunk, timeout_ms,
                xsink, true);
            if (write_rc == -1 || *xsink) {
                return nullptr;
            }
        } else if (!recv_callback && binary_body) {
            body_bin->append(chunk->getPtr(), chunk->size());
        } else if (!recv_callback) {
            body_str->concat(static_cast<const char*>(chunk->getPtr()), chunk->size());
        }

        if (!os) {
            int64 body_size = recv_callback
                ? static_cast<int64>(chunk->size())
                : binary_body ? static_cast<int64>(body_bin->size()) : static_cast<int64>(body_str->size());
            int64 max_chunked_body_size = priv->getMaxChunkedBodySize();
            if (max_chunked_body_size > 0 && body_size > max_chunked_body_size) {
                xsink->raiseException("HTTP-BODY-TOO-LARGE", "chunked body size " QLLD " exceeds maximum " QLLD,
                    body_size, max_chunked_body_size);
                return nullptr;
            }
        }

        SimpleRefHolder<BinaryNode> crlf(qore_socket_object_exec_recv_bytes(s, 2, timeout_ms, xsink));
        if (*xsink) {
            return nullptr;
        }
        priv->doChunkedReadEvent(QORE_EVENT_HTTP_CHUNKED_DATA_RECEIVED, static_cast<size_t>(chunk_size),
            static_cast<size_t>(chunk_size) + 2, QORE_SOURCE_SOCKET);

        if (recv_callback) {
            if (binary_body) {
                if (qore_socket_object_exec_run_http_recv_data_callback(recv_callback, **chunk, xsink)) {
                    return nullptr;
                }
            } else {
                SimpleRefHolder<QoreStringNode> chunk_str(new QoreStringNode(
                    static_cast<const char*>(chunk->getPtr()), chunk->size(), s->getEncoding()));
                if (qore_socket_object_exec_run_http_recv_data_callback(recv_callback, **chunk_str, xsink)) {
                    return nullptr;
                }
            }
        }

        if (read_once) {
            ReferenceHolder<QoreHashNode> output(new QoreHashNode(autoTypeInfo), xsink);
            assert(binary_body);
            output->setKeyValue("body", body_bin.release(), xsink);
            return *xsink ? nullptr : output.release();
        }
    }
}

static QoreHashNode* qore_socket_object_exec_read_server_sent_event(QoreSocketObject* s, int timeout_ms,
        ExceptionSink* xsink) {
    s->ref();
    ValueHolder rv(qore_socket_object_exec_recv_poll(s,
        new SocketReadServerSentEventPollOperation(xsink, s, true), timeout_ms, "readServerSentEvent", xsink),
        xsink);
    if (*xsink) {
        return nullptr;
    }
    if (rv->getType() != NT_HASH) {
        xsink->raiseException("SOCKET-SSE-ERROR",
            "expected hash output from async SSE read operation, got '%s'", rv->getFullTypeName());
        return nullptr;
    }
    return rv.release().get<QoreHashNode>();
}

static QoreHashNode* qore_socket_object_exec_read_server_sent_event_encoded(QoreSocketObject* s,
        const QoreStringNode* content_encoding, int timeout_ms, ExceptionSink* xsink) {
    s->ref();
    ValueHolder rv(qore_socket_object_exec_recv_poll(s,
        new SocketReadServerSentEventPollOperation(xsink, s, content_encoding, true), timeout_ms,
        "readServerSentEvent", xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    if (rv->getType() != NT_HASH) {
        xsink->raiseException("SOCKET-SSE-ERROR",
            "expected hash output from async SSE read operation, got '%s'", rv->getFullTypeName());
        return nullptr;
    }
    return rv.release().get<QoreHashNode>();
}

static int qore_socket_object_exec_send_http_message(QoreSocketObject* s, QoreHashNode* info,
        const char* method, const char* path, const char* http_version, const QoreHashNode* headers,
        const void* data, size_t size, const QoreStringNode* body_event, int source, int timeout_ms,
        ExceptionSink* xsink) {
    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_SEND);
    if (!async_guard) {
        return -1;
    }

    if (qore_socket_object_exec_check_http1_allowed(s, "sendHTTPMessage", xsink)) {
        return -1;
    }

    QoreString hdr(s->getEncoding());
    if (priv->getSendHttpMessageHeaders(xsink, hdr, info, method, path, http_version, headers, size, source)) {
        return -1;
    }

    SimpleRefHolder<BinaryNode> msg(new BinaryNode());
    msg->append(hdr.c_str(), hdr.size());
    if (size && data) {
        msg->append(data, size);
    }

    int rc = qore_socket_object_exec_send_binary(s, msg.release(), timeout_ms, xsink);
    if (!rc && size && data) {
        if (body_event) {
            my_socket_priv::getPriv(*s)->doDataEvent(QORE_EVENT_SOCKET_DATA_SENT, source, *body_event);
        } else {
            my_socket_priv::getPriv(*s)->doDataEvent(QORE_EVENT_SOCKET_DATA_SENT, source, data, size);
        }
    }
    return rc;
}

static int qore_socket_object_exec_send_http_response(QoreSocketObject* s, QoreHashNode* info,
        int code, const char* desc, const char* http_version, const QoreHashNode* headers,
        const void* data, size_t size, const QoreStringNode* body_event, int source, int timeout_ms,
        ExceptionSink* xsink) {
    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_SEND);
    if (!async_guard) {
        return -1;
    }

    int32_t stream_id = priv->getH2ActiveThreadStreamId();
    stream_id = qore_socket_object_exec_get_http2_server_stream(s, stream_id, "sendHTTPResponse", xsink);
    if (*xsink) {
        return -1;
    }
    if (stream_id > 0) {
        QoreString status_line(s->getEncoding());
        priv->getSendHttpResponseStatusLine(status_line, info, code, desc, http_version);

        SimpleRefHolder<BinaryNode> body_bin;
        if (size && data) {
            body_bin = new BinaryNode;
            body_bin->append(data, size);
        }

        s->ref();
        return qore_socket_object_exec_send_poll(s,
            new SocketHttp2SendResponsePollOperation(xsink, s, nullptr, stream_id, code, headers, *body_bin, false,
                true),
            timeout_ms, xsink, NB_ALL);
    }

    QoreString hdr(s->getEncoding());
    if (priv->getSendHttpResponseHeaders(xsink, hdr, info, code, desc, http_version, headers, size, source)) {
        return -1;
    }

    SimpleRefHolder<BinaryNode> msg(new BinaryNode());
    msg->append(hdr.c_str(), hdr.size());
    if (size && data) {
        msg->append(data, size);
    }

    int rc = qore_socket_object_exec_send_binary(s, msg.release(), timeout_ms, xsink);
    if (!rc && size && data) {
        if (body_event) {
            priv->doDataEvent(QORE_EVENT_SOCKET_DATA_SENT, source, *body_event);
        } else {
            priv->doDataEvent(QORE_EVENT_SOCKET_DATA_SENT, source, data, size);
        }
    }
    return rc;
}

static int qore_socket_object_exec_send_http_chunked_body_trailer(QoreSocketObject* s,
        const QoreHashNode* trailer, int source, int timeout_ms, ExceptionSink* xsink) {
    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_SEND);
    if (!async_guard) {
        return -1;
    }

    QoreString hdr(s->getEncoding());
    hdr.concat("0\r\n");
    qore_socket_private::do_headers(hdr, trailer, 0, false);

    int rc = qore_socket_object_exec_send_bytes(s, hdr.c_str(), hdr.size(), timeout_ms, xsink);
    if (!rc && trailer) {
        priv->doHeaderEvent(QORE_EVENT_HTTP_FOOTERS_SENT, source, *trailer);
    }
    return rc;
}

static void qore_socket_object_exec_set_http_chunk_prefix(QoreString& prefix, size_t size) {
    prefix.sprintf(QLLX "\r\n", static_cast<unsigned long long>(size));
}

static int qore_socket_object_exec_run_http_trailer_callback(
        const ResolvedCallReferenceNode* trailer_callback, ReferenceHolder<QoreHashNode>& trailer,
        ExceptionSink* xsink) {
    if (!trailer_callback) {
        return 0;
    }

    ValueHolder rv(trailer_callback->execValue(nullptr, xsink), xsink);
    if (*xsink) {
        return -1;
    }

    switch (rv->getType()) {
        case NT_NOTHING:
            break;
        case NT_HASH:
            trailer = rv.release().get<QoreHashNode>();
            break;
        default:
            xsink->raiseException("HTTP-TRAILER-ERROR", "chunked callback returned type '%s'; expecting 'hash' "
                "or 'NOTHING'", rv->getTypeName());
            return -1;
    }
    return 0;
}

static int qore_socket_object_exec_send_http_chunked_body_input_stream(QoreSocketObject* s,
        InputStream* input_stream, size_t max_chunk_size, const ResolvedCallReferenceNode* trailer_callback,
        int timeout_ms, ExceptionSink* xsink) {
    SocketSyncPoll::assertNotOnIoThread("Socket", "sendHTTPChunkedBodyFromInputStream", xsink);
    if (*xsink) {
        return -1;
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_SEND);
    if (!async_guard) {
        return -1;
    }

    int rc = qore_socket_object_exec_send_http_chunked_body_input_stream_poll(s, input_stream, nullptr,
        max_chunk_size, timeout_ms, !trailer_callback, xsink, true);
    if (rc) {
        return -1;
    }
    if (!trailer_callback) {
        return 0;
    }

    ReferenceHolder<QoreHashNode> trailer(xsink);
    if (qore_socket_object_exec_run_http_trailer_callback(trailer_callback, trailer, xsink)) {
        return -1;
    }
    if (trailer) {
        return qore_socket_object_exec_send_http_chunked_body_trailer(s, *trailer, QORE_SOURCE_SOCKET,
            timeout_ms, xsink);
    }
    return qore_socket_object_exec_send_bytes(s, "0\r\n\r\n", 5, timeout_ms, xsink);
}

static bool qore_socket_object_exec_is_data_available(QoreSocketObject* s, int timeout_ms, ExceptionSink* xsink);

static bool qore_socket_object_exec_check_send_aborted(QoreSocketObject* s, bool* aborted, ExceptionSink* xsink) {
    if (!aborted) {
        return false;
    }

    bool data_available = qore_socket_object_exec_is_data_available(s, 0, xsink);
    if (data_available || *xsink) {
        *aborted = true;
        return true;
    }
    return false;
}

static bool qore_socket_object_exec_try_clear_send_error_as_aborted(QoreSocketObject* s, bool* aborted,
        ExceptionSink* xsink) {
    if (!aborted || !*xsink) {
        return false;
    }

    ExceptionSink aborted_xsink;
    bool data_available = qore_socket_object_exec_is_data_available(s, 0, &aborted_xsink);
    if (aborted_xsink) {
        aborted_xsink.clear();
        return false;
    }
    if (!data_available) {
        return false;
    }

    xsink->clear();
    *aborted = true;
    return true;
}

static int qore_socket_object_exec_send_http_chunked_body_callback(QoreSocketObject* s,
        const ResolvedCallReferenceNode* send_callback, int source, int timeout_ms, bool* aborted,
        ExceptionSink* xsink) {
    assert(!aborted || !(*aborted));

    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_SEND);
    if (!async_guard) {
        return -1;
    }

    unsigned cancel_check = 0;
    while (true) {
        if (!(cancel_check++ % 100) && qore_check_cancel(xsink, "socket chunked callback send")) {
            return -1;
        }

        if (qore_socket_object_exec_check_send_aborted(s, aborted, xsink)) {
            return *xsink ? -1 : 0;
        }

        ValueHolder res(send_callback->execValue(nullptr, xsink), xsink);
        if (*xsink) {
            return -1;
        }

        const void* data = nullptr;
        size_t size = 0;
        const QoreStringNode* body_event = nullptr;
        bool done = false;

        switch (res->getType()) {
            case NT_STRING: {
                const QoreStringNode* str = res->get<const QoreStringNode>();
                if (str->empty()) {
                    done = true;
                    break;
                }
                data = str->c_str();
                size = str->size();
                body_event = str;
                break;
            }
            case NT_BINARY: {
                const BinaryNode* bin = res->get<const BinaryNode>();
                if (bin->empty()) {
                    done = true;
                    break;
                }
                data = bin->getPtr();
                size = bin->size();
                break;
            }
            case NT_HASH:
                if (qore_socket_object_exec_send_http_chunked_body_trailer(s, res->get<const QoreHashNode>(),
                        source, timeout_ms, xsink)
                        && !qore_socket_object_exec_try_clear_send_error_as_aborted(s, aborted, xsink)) {
                    return -1;
                }
                return 0;
            case NT_NOTHING:
            case NT_NULL:
                done = true;
                break;
            default:
                xsink->raiseException("SOCKET-CALLBACK-ERROR", "HTTP chunked data callback returned type '%s'; "
                    "expecting one of: 'string', 'binary', 'hash', 'nothing' (or 'NULL')", res->getTypeName());
                return -1;
        }

        if (done) {
            if (qore_socket_object_exec_send_bytes(s, "0\r\n\r\n", 5, timeout_ms, xsink)
                    && !qore_socket_object_exec_try_clear_send_error_as_aborted(s, aborted, xsink)) {
                return -1;
            }
            return 0;
        }

        QoreString prefix;
        qore_socket_object_exec_set_http_chunk_prefix(prefix, size);
        if (qore_socket_object_exec_send_bytes(s, prefix.c_str(), prefix.size(), timeout_ms, xsink)
                || qore_socket_object_exec_send_bytes(s, data, size, timeout_ms, xsink)
                || qore_socket_object_exec_send_bytes(s, "\r\n", 2, timeout_ms, xsink)) {
            return qore_socket_object_exec_try_clear_send_error_as_aborted(s, aborted, xsink) ? 0 : -1;
        }

        if (body_event) {
            priv->doDataEvent(QORE_EVENT_HTTP_CHUNKED_DATA_SENT, source, *body_event);
        } else {
            priv->doDataEvent(QORE_EVENT_HTTP_CHUNKED_DATA_SENT, source, data, size);
        }
    }
}

static int qore_socket_object_exec_send_http_message_callback(QoreSocketObject* s, QoreHashNode* info,
        const char* method, const char* path, const char* http_version, const QoreHashNode* headers,
        const ResolvedCallReferenceNode* send_callback, int source, int timeout_ms, bool* aborted,
        ExceptionSink* xsink) {
    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_SEND);
    if (!async_guard) {
        return -1;
    }

    if (qore_socket_object_exec_check_http1_allowed(s, "sendHTTPMessageWithCallback", xsink)) {
        return -1;
    }

    QoreString hdr(s->getEncoding());
    if (priv->getSendHttpMessageChunkedHeaders(xsink, hdr, info, method, path, http_version, headers, source)) {
        return -1;
    }

    if (qore_socket_object_exec_send_bytes(s, hdr.c_str(), hdr.size(), timeout_ms, xsink)) {
        return -1;
    }
    return qore_socket_object_exec_send_http_chunked_body_callback(s, send_callback, source, timeout_ms, aborted,
        xsink);
}

static int qore_socket_object_exec_send_http_response_callback(QoreSocketObject* s, QoreHashNode* info,
        int code, const char* desc, const char* http_version, const QoreHashNode* headers,
        const ResolvedCallReferenceNode* send_callback, int source, int timeout_ms, bool* aborted,
        ExceptionSink* xsink) {
    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_SEND);
    if (!async_guard) {
        return -1;
    }

    int32_t stream_id = priv->getH2ActiveThreadStreamId();
    stream_id = qore_socket_object_exec_get_http2_server_stream(s, stream_id, "sendHTTPResponseWithCallback",
        xsink);
    if (*xsink) {
        return -1;
    }

    QoreString hdr(s->getEncoding());
    if (stream_id > 0) {
        priv->getSendHttpResponseStatusLine(hdr, info, code, desc, http_version);
        xsink->raiseException("HTTP2-ERROR",
            "chunked/streaming responses are not supported via Socket::sendHTTPResponse() on HTTP/2");
        return -1;
    }
    if (priv->getSendHttpResponseChunkedHeaders(xsink, hdr, info, code, desc, http_version, headers, source)) {
        return -1;
    }

    if (qore_socket_object_exec_send_bytes(s, hdr.c_str(), hdr.size(), timeout_ms, xsink)) {
        return -1;
    }
    return qore_socket_object_exec_send_http_chunked_body_callback(s, send_callback, source, timeout_ms, aborted,
        xsink);
}

static int qore_socket_object_exec_send_http_response_input_stream(QoreSocketObject* s, QoreHashNode* info,
        int code, const char* desc, const char* http_version, const QoreHashNode* headers, InputStream* input_stream,
        size_t max_chunk_size, const ResolvedCallReferenceNode* trailer_callback, int source, int timeout_ms,
        ExceptionSink* xsink) {
    if (!input_stream->isIoThreadSafe()) {
        xsink->raiseException("SOCKET-SEND-ERROR", "InputStream is not I/O thread safe");
        return -1;
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_SEND);
    if (!async_guard) {
        return -1;
    }

    int32_t stream_id = priv->getH2ActiveThreadStreamId();
    stream_id = qore_socket_object_exec_get_http2_server_stream(s, stream_id, "sendHTTPResponse", xsink);
    if (*xsink) {
        return -1;
    }
    if (stream_id > 0) {
        QoreString status_line(s->getEncoding());
        priv->getSendHttpResponseStatusLine(status_line, info, code, desc, http_version);
        xsink->raiseException("HTTP2-ERROR",
            "chunked/streaming responses are not supported via Socket::sendHTTPResponse() on HTTP/2");
        return -1;
    }

    QoreString hdr(s->getEncoding());
    if (priv->getSendHttpResponseHeaders(xsink, hdr, info, code, desc, http_version, headers, 0, source)) {
        return -1;
    }

    if (qore_socket_object_exec_send_bytes(s, hdr.c_str(), hdr.size(), timeout_ms, xsink)) {
        return -1;
    }
    return qore_socket_object_exec_send_http_chunked_body_input_stream(s, input_stream, max_chunk_size,
        trailer_callback, timeout_ms, xsink);
}

static int qore_socket_object_exec_recv_fd(QoreSocketObject* s, int fd, int size, int timeout_ms) {
    if (!size) {
        return 0;
    }

    ExceptionSink xsink;
    SimpleRefHolder<FileOutputStream> os(new FileOutputStream(fd));
    int rc = qore_socket_object_exec_recv_output_stream_poll(s, *os, nullptr, size, timeout_ms, &xsink, false, false);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

static bool qore_socket_object_exec_wait_readiness(QoreSocketObject* s, int timeout_ms, unsigned direction,
        int events, const char* owner_name, const char* waiting_state, const char* ready_state,
        ExceptionSink* xsink) {
    s->ref();
    QoreSocketObjectReadinessPollOperation* readiness_poller = new QoreSocketObjectReadinessPollOperation(xsink, s,
        direction, events, waiting_state, ready_state);
    ReferenceHolder<SocketPollOperationBase> poller(readiness_poller, xsink);
    if (*xsink) {
        return false;
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, direction);
    if (!async_guard) {
        return false;
    }

    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, poller.release(), owner_name, xsink), xsink);
    if (*xsink) {
        return false;
    }

    QoreHashNode* ex = nullptr;
    ReferenceHolder<QoreHashNode> result(qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj, timeout_ms,
        owner_name, xsink, &ex), xsink);
    ReferenceHolder<QoreHashNode> ex_holder(ex, xsink);
    if (*xsink) {
        return false;
    }
    if (ex_holder) {
        if (qore_socket_object_exec_exception_is(**ex_holder, "SOCKET-TIMEOUT")) {
            return false;
        }
        qore_socket_object_raise_poll_result_exception(*ex_holder, xsink);
        return false;
    }

    return readiness_poller->goalReached();
}

static bool qore_socket_object_exec_is_data_available(QoreSocketObject* s, int timeout_ms,
        ExceptionSink* xsink) {
    s->ref();
    SocketDataAvailablePollOperation* data_available_poller = new SocketDataAvailablePollOperation(xsink, s, true);
    ReferenceHolder<SocketPollOperationBase> poller(data_available_poller, xsink);
    if (*xsink) {
        return false;
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_RECV);
    if (!async_guard) {
        return false;
    }

    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, poller.release(), "isDataAvailable", xsink), xsink);
    if (*xsink) {
        return false;
    }

    QoreHashNode* ex = nullptr;
    ReferenceHolder<QoreHashNode> result(qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj, timeout_ms,
        "isDataAvailable", xsink, &ex), xsink);
    ReferenceHolder<QoreHashNode> ex_holder(ex, xsink);
    if (*xsink) {
        return false;
    }
    if (ex_holder) {
        if (qore_socket_object_exec_exception_is(**ex_holder, "SOCKET-TIMEOUT")) {
            return false;
        }
        qore_socket_object_raise_poll_result_exception(*ex_holder, xsink);
        return false;
    }

    return data_available_poller->goalReached();
}

static int qore_socket_object_exec_check_idle_data(QoreSocketObject* s, ExceptionSink* xsink) {
    s->ref();
    QoreSocketObjectIdleDataPollOperation* idle_poller = new QoreSocketObjectIdleDataPollOperation(xsink, s);
    ReferenceHolder<SocketPollOperationBase> poller(idle_poller, xsink);
    if (*xsink) {
        return -1;
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_RECV);
    if (!async_guard) {
        return -1;
    }

    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, poller.release(), "check-idle-data", xsink), xsink);
    if (*xsink) {
        return -1;
    }

    ReferenceHolder<QoreHashNode> result(qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj, 0,
        "checkIdleData", xsink), xsink);
    if (*xsink) {
        return -1;
    }

    return idle_poller->getOutput().getAsBigInt();
}

QoreSocketObject::QoreSocketObject(QoreSocket* s, QoreSSLCertificate* cert, QoreSSLPrivateKey* pk)
        : priv(new my_socket_priv(s, cert, pk)) {
}

QoreSocketObject::QoreSocketObject(QoreSocketObject& orig, int descriptor)
        : priv(new my_socket_priv(new QoreSocket(descriptor, orig.priv->socket->priv->sfamily,
            orig.priv->socket->priv->stype,
            orig.priv->socket->priv->sprot, orig.priv->socket->priv->enc),
            orig.priv->cert ? orig.priv->cert->certRefSelf() : nullptr,
            orig.priv->pk ? orig.priv->pk->pkRefSelf() : nullptr)) {
    qore_socket_private* src = orig.priv->socket->priv;
    qore_socket_private* dst = priv->socket->priv;

    if (!src->socketname.empty()) {
        dst->socketname = src->socketname;
    }
    dst->setSslVerifyMode(src->ssl_verify_mode);
    dst->acceptAllCertificates(src->ssl_accept_all_certs);
    if (src->ssl_capture_remote_cert) {
        dst->ssl_capture_remote_cert = true;
    }
    // Copy ALPN protocols from the listener socket to the accepted socket so that
    // SSL handshakes on the accepted socket correctly negotiate HTTP/2 via ALPN
    if (!src->alpn_protocols.empty()) {
        dst->alpn_protocols = src->alpn_protocols;
    }
}

QoreSocketObject::QoreSocketObject() : priv(new my_socket_priv) {
}

QoreSocketObject::~QoreSocketObject() {
    delete priv;
}

void QoreSocketObject::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        priv->socket->cleanup(xsink);
        delete this;
    }
}

void QoreSocketObject::deref() {
    if (ROdereference()) {
        ExceptionSink xsink;
        priv->socket->cleanup(&xsink);
        delete this;
    }
}

void QoreSocketObject::closeIo(ExceptionSink* xsink) {
    priv->socket->priv->prepareForClose();
    AutoLocker al(priv->m);
    if (priv->socket->isOpen()) {
        qore_socket_private::get(*priv->socket)->shutdown_direct();
        priv->socket->priv->close();
    }
}

bool QoreSocketObject::hasPendingData() const {
    // Check the socket's internal read buffer for unread data left over from
    // a previous read operation.  This handles protocol upgrades (HTTP→WS)
    // where SSL_read() reads a full TLS record containing both HTTP headers
    // and the first WebSocket frame; the header reader consumes only headers,
    // leaving frame data in rbuf that the next poll op must process.
    //
    // buflen is the LENGTH of unread data starting at bufoffset (see brecv()
    // and readByteFromBuffer() in qore_socket_private.h) — comparing it to
    // bufoffset as if it were an end-index silently drops leftover bytes
    // whenever a previous reader has advanced bufoffset.
    if (priv->socket->priv->buflen) {
        return true;
    }
    // Check SSL buffer without syscalls or locking — safe from any thread
    // SSL_pending() is documented as thread-safe for read-only queries
    if (priv->socket->priv->ssl && priv->socket->priv->ssl->pending() > 0) {
        return true;
    }
    // Check H2/H3 session for pending data at the protocol level:
    // - hasStreamData(): CONNECT stream DATA received in a previous read cycle
    //   but not yet drained by the poll operation
    // - wantWrite(): nghttp2 has outgoing frames (SETTINGS_ACK, WINDOW_UPDATE)
    //   OR bytes buffered in our send_buffer waiting to go out
    // Without these checks, the SSL/TCP buffers appear empty so epoll never
    // triggers a re-poll, but the H2 session has actionable work.
    //
    // IMPORTANT: do NOT check nghttp2_session_want_read() here.  That API
    // returns non-zero for any alive H2 session (the default state when the
    // session is open and not closing), so treating it as "pending data"
    // causes a tight polling loop on every idle H2 connection.  Actual
    // inbound data waiting to be processed shows up in SSL_pending() /
    // buflen (checked above) — not in nghttp2's internal state.
    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    if (h2 && (h2->hasStreamData() || h2->wantWrite())) {
        return true;
    }
    return false;
}

BinaryNode* QoreSocketObject::drainPendingBuffer() {
    qore_socket_private* sp = priv->socket->priv;
    // buflen is the LENGTH of unread data starting at bufoffset (see brecv()
    // and readByteFromBuffer() in qore_socket_private.h).  Earlier code here
    // computed avail = buflen - bufoffset which silently dropped leftover
    // bytes whenever the previous reader had advanced bufoffset, e.g. an
    // HTTP/1.1 client that consumed a 101 response and left WS frame bytes
    // in the buffer for the WebSocket poll op to pick up.
    if (!sp->buflen) {
        return nullptr;
    }
    // BinaryNode takes ownership of its pointer, so we must allocate a copy
    // — sp->rbuf is owned by the socket and cannot be free()d by the node.
    BinaryNode* result = new BinaryNode();
    result->append(sp->rbuf + sp->bufoffset, sp->buflen);
    sp->bufoffset = 0;
    sp->buflen = 0;
    return result;
}

void QoreSocketObject::invalidate(ExceptionSink* xsink) {
    {
        AutoLocker al(priv->m);
        priv->invalidate();
    }
    priv->socket->cleanup(xsink);
}

AbstractPollState* QoreSocketObject::startConnect(ExceptionSink* xsink, const char* name) {
    AutoLocker al(priv->m);
    if (priv->checkValid(xsink)) {
        return nullptr;
    }
    return priv->socket->startConnect(xsink, name);
}

AbstractPollState* QoreSocketObject::startSslConnect(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkValid(xsink)) {
        return nullptr;
    }
    return priv->socket->startSslConnect(xsink, priv->cert, priv->pk);
}

AbstractPollState* QoreSocketObject::startSend(ExceptionSink* xsink, const char* data, size_t size) {
    AutoLocker al(priv->m);
    if (priv->checkValid(xsink)) {
        return nullptr;
    }
    return priv->socket->startSend(xsink, data, size);
}

AbstractPollState* QoreSocketObject::startSslAccept(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkValid(xsink)) {
        return nullptr;
    }
    return priv->socket->startSslAccept(xsink, priv->cert, priv->pk);
}

AbstractPollState* QoreSocketObject::startRecv(ExceptionSink* xsink, size_t size) {
    AutoLocker al(priv->m);
    if (priv->checkValid(xsink)) {
        return nullptr;
    }
    return priv->socket->startRecv(xsink, size);
}

AbstractPollState* QoreSocketObject::startRecvUntilBytes(ExceptionSink* xsink, const char* pattern, size_t size) {
    AutoLocker al(priv->m);
    if (priv->checkValid(xsink)) {
        return nullptr;
    }
    return priv->socket->startRecvUntilBytes(xsink, pattern, size);
}

AbstractPollState* QoreSocketObject::startRecvPacket(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkValid(xsink)) {
        return nullptr;
    }
    return priv->socket->startRecvPacket(xsink);
}

AbstractPollState* QoreSocketObject::startAccept(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkValid(xsink)) {
        return nullptr;
    }
    return priv->socket->startAccept(xsink);
}

int QoreSocketObject::connect(const char* name, int timeout_ms, ExceptionSink* xsink) {
    return qore_socket_object_exec_connect(this, name, timeout_ms, false, xsink);
}

int QoreSocketObject::connectINET(const char* host, int port, int timeout_ms, ExceptionSink* xsink) {
    QoreString service;
    service.sprintf("%d", port);
    return qore_socket_object_exec_connect_inet(this, host, service.c_str(), AF_UNSPEC, SOCK_STREAM, 0, timeout_ms,
        false, xsink);
}

int QoreSocketObject::connectINET2(const char* name, const char* service, int family, int sock_type, int protocol,
        int timeout_ms, ExceptionSink* xsink) {
    return qore_socket_object_exec_connect_inet(this, name, service, family, sock_type, protocol, timeout_ms, false,
        xsink);
}

int QoreSocketObject::connectUNIX(const char* p, int sock_type, int protocol, ExceptionSink* xsink) {
    return qore_socket_object_exec_connect_unix(this, p, sock_type, protocol, -1, false, xsink);
}

// to bind to either a UNIX socket or an INET interface:port
int QoreSocketObject::bind(const char* name, bool reuseaddr) {
    ExceptionSink xsink;
    int rc = qore_socket_object_exec_setup(this, new SocketSetupPollOperation(&xsink, this, name, reuseaddr),
        "bind", "bind", &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

// to bind to an INET tcp port on all interfaces
int QoreSocketObject::bind(int port, bool reuseaddr) {
    ExceptionSink xsink;
    int rc = qore_socket_object_exec_setup(this, new SocketSetupPollOperation(&xsink, this, port, reuseaddr),
        "bind", "bind", &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

// to bind an open socket to an INET tcp port on a specific interface
int QoreSocketObject::bind(const char* iface, int port, bool reuseaddr) {
    ExceptionSink xsink;
    int rc = qore_socket_object_exec_setup(this, new SocketSetupPollOperation(&xsink, this, iface, port, reuseaddr),
        "bind", "bind", &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

int QoreSocketObject::bindUNIX(const char* name, int socktype, int protocol, ExceptionSink* xsink) {
    return qore_socket_object_exec_setup(this, new SocketSetupPollOperation(xsink, this, name, socktype, protocol),
        "bind", "bind", xsink);
}

int QoreSocketObject::bindINET(const char* name, const char* service, bool reuseaddr, int family, int socktype,
        int protocol, ExceptionSink* xsink) {
    return qore_socket_object_exec_setup(this, new SocketSetupPollOperation(xsink, this, name, service, reuseaddr,
        family, socktype, protocol), "bind", "bind", xsink);
}

// get port number for INET sockets
int QoreSocketObject::getPort() {
    ExceptionSink xsink;
    int rc = qore_socket_object_exec_setup(this, new SocketSetupPollOperation(&xsink, this,
        SocketSetupPollOperation::ConfigAction::GetPort), "getPort", "done", &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

int QoreSocketObject::listen(int backlog) {
    ExceptionSink xsink;
    int rc = qore_socket_object_exec_setup(this, new SocketSetupPollOperation(&xsink, this, backlog), "listen",
        "listen", &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

// send a buffer of a particular size
int QoreSocketObject::send(const char* buf, int size) {
    ExceptionSink xsink;
    int rc = qore_socket_object_exec_send_bytes(this, buf, size, -1, &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

int QoreSocketObject::send(const char* buf, int size, int timeout_ms, ExceptionSink* xsink) {
    return qore_socket_object_exec_send_bytes(this, buf, size, timeout_ms, xsink);
}

// send a null-terminated string
int QoreSocketObject::send(const QoreStringNode& msg, int timeout_ms, ExceptionSink* xsink) {
    QoreStringNode* sent_data = nullptr;
    int rc = qore_socket_object_exec_send_string(this, msg, timeout_ms, xsink, &sent_data);
    SimpleRefHolder<QoreStringNode> sent_data_holder(sent_data);
    if (!rc) {
        priv->socket->priv->do_data_event(QORE_EVENT_SOCKET_DATA_SENT, QORE_SOURCE_SOCKET, **sent_data_holder);
    }
    return rc;
}

// send a binary object
int QoreSocketObject::send(const BinaryNode* b, int timeout_ms, ExceptionSink* xsink) {
    int rc = qore_socket_object_exec_send_binary(this, b->binRefSelf(), timeout_ms, xsink);
    if (!rc) {
        priv->socket->priv->do_data_event(QORE_EVENT_SOCKET_DATA_SENT, QORE_SOURCE_SOCKET, *b);
    }
    return rc;
}

int QoreSocketObject::send(const BinaryNode* b) {
    ExceptionSink xsink;
    int rc = send(b, -1, &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

void QoreSocketObject::sendFromInputStream(InputStream *is, int64 size, int64 timeout_ms, ExceptionSink *xsink) {
    SocketSyncPoll::assertNotOnIoThread("Socket", "sendFromInputStream", xsink);
    if (*xsink) {
        return;
    }

    qore_socket_object_exec_send_input_stream_poll(this, is, nullptr, size, static_cast<int>(timeout_ms), xsink,
        true);
}

// send from a file descriptor
int QoreSocketObject::send(int fd, int size) {
    ExceptionSink xsink;
    SimpleRefHolder<FileInputStream> is(new FileInputStream(fd));
    int rc = qore_socket_object_exec_send_input_stream_poll(this, *is, nullptr, size, -1, &xsink, false);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

// send bytes and convert to network order
int QoreSocketObject::sendi1(char b, int timeout_ms, ExceptionSink* xsink) {
    return qore_socket_object_exec_send_bytes(this, &b, sizeof(b), timeout_ms, xsink);
}

int QoreSocketObject::sendi2(short b, int timeout_ms, ExceptionSink* xsink) {
    b = htons(b);
    return qore_socket_object_exec_send_bytes(this, &b, sizeof(b), timeout_ms, xsink);
}

int QoreSocketObject::sendi4(int b, int timeout_ms, ExceptionSink* xsink) {
    b = htonl(b);
    return qore_socket_object_exec_send_bytes(this, &b, sizeof(b), timeout_ms, xsink);
}

int QoreSocketObject::sendi8(int64 b, int timeout_ms, ExceptionSink* xsink) {
    b = i8MSB(b);
    return qore_socket_object_exec_send_bytes(this, &b, sizeof(b), timeout_ms, xsink);
}

int QoreSocketObject::sendi2LSB(short b, int timeout_ms, ExceptionSink* xsink) {
    b = i2LSB(b);
    return qore_socket_object_exec_send_bytes(this, &b, sizeof(b), timeout_ms, xsink);
}

int QoreSocketObject::sendi4LSB(int b, int timeout_ms, ExceptionSink* xsink) {
    b = i4LSB(b);
    return qore_socket_object_exec_send_bytes(this, &b, sizeof(b), timeout_ms, xsink);
}

int QoreSocketObject::sendi8LSB(int64 b, int timeout_ms, ExceptionSink* xsink) {
    b = i8LSB(b);
    return qore_socket_object_exec_send_bytes(this, &b, sizeof(b), timeout_ms, xsink);
}

// receive a packet of bytes as a string
QoreStringNode* QoreSocketObject::recv(int timeout_ms, ExceptionSink* xsink) {
    SimpleRefHolder<QoreStringNode> str(qore_socket_object_exec_recv_string(this, 0, timeout_ms, xsink));
    if (*xsink) {
        return nullptr;
    }
    priv->socket->priv->do_data_event(QORE_EVENT_SOCKET_DATA_READ, QORE_SOURCE_SOCKET, **str);
    return str.release();
}

// receive a certain number of bytes as a string
QoreStringNode* QoreSocketObject::recv(qore_offset_t bufsize, int timeout_ms, ExceptionSink* xsink) {
    SimpleRefHolder<QoreStringNode> str(qore_socket_object_exec_recv_string(this, bufsize, timeout_ms, xsink));
    if (*xsink) {
        return nullptr;
    }
    priv->socket->priv->do_data_event(QORE_EVENT_SOCKET_DATA_READ, QORE_SOURCE_SOCKET, **str);
    return str.release();
}

// receive a packet of bytes as a binary
BinaryNode* QoreSocketObject::recvBinary(int timeout_ms, ExceptionSink* xsink) {
    SimpleRefHolder<BinaryNode> bin(qore_socket_object_exec_recv_binary(this, 0, timeout_ms, xsink));
    if (*xsink) {
        return nullptr;
    }
    priv->socket->priv->do_data_event(QORE_EVENT_SOCKET_DATA_READ, QORE_SOURCE_SOCKET, **bin);
    return bin.release();
}

// receive a certain number of bytes as a binary object
BinaryNode* QoreSocketObject::recvBinary(int bufsize, int timeout_ms, ExceptionSink* xsink) {
    SimpleRefHolder<BinaryNode> bin(qore_socket_object_exec_recv_binary(this, bufsize, timeout_ms, xsink));
    if (*xsink) {
        return nullptr;
    }
    priv->socket->priv->do_data_event(QORE_EVENT_SOCKET_DATA_READ, QORE_SOURCE_SOCKET, **bin);
    return bin.release();
}

void QoreSocketObject::recvToOutputStream(OutputStream *os, int64 size, int64 timeout_ms, ExceptionSink *xsink) {
    SocketSyncPoll::assertNotOnIoThread("Socket", "recvToOutputStream", xsink);
    if (*xsink) {
        return;
    }

    qore_socket_object_exec_recv_output_stream_poll(this, os, nullptr, size, static_cast<int>(timeout_ms), xsink,
        true, true);
}

// receive and write data to a file descriptor
int QoreSocketObject::recv(int fd, int size, int timeout_ms) {
    return qore_socket_object_exec_recv_fd(this, fd, size, timeout_ms);
}

// receive integers and convert from network byte order
int64 QoreSocketObject::recvi1(int timeout_ms, char* b, ExceptionSink* xsink) {
    SimpleRefHolder<BinaryNode> data(qore_socket_object_exec_recv_bytes(this, sizeof(*b), timeout_ms, xsink));
    if (*xsink) {
        return -1;
    }
    memcpy(b, data->getPtr(), sizeof(*b));
    priv->socket->priv->do_data_event(QORE_EVENT_SOCKET_DATA_READ, QORE_SOURCE_SOCKET, **data);
    return sizeof(*b);
}

int64 QoreSocketObject::recvi2(int timeout_ms, short* b, ExceptionSink* xsink) {
    SimpleRefHolder<BinaryNode> data(qore_socket_object_exec_recv_bytes(this, sizeof(*b), timeout_ms, xsink));
    if (*xsink) {
        return -1;
    }
    memcpy(b, data->getPtr(), sizeof(*b));
    priv->socket->priv->do_data_event(QORE_EVENT_SOCKET_DATA_READ, QORE_SOURCE_SOCKET, **data);
    *b = ntohs(*b);
    return sizeof(*b);
}

int64 QoreSocketObject::recvi4(int timeout_ms, int* b, ExceptionSink* xsink) {
    SimpleRefHolder<BinaryNode> data(qore_socket_object_exec_recv_bytes(this, sizeof(*b), timeout_ms, xsink));
    if (*xsink) {
        return -1;
    }
    memcpy(b, data->getPtr(), sizeof(*b));
    priv->socket->priv->do_data_event(QORE_EVENT_SOCKET_DATA_READ, QORE_SOURCE_SOCKET, **data);
    *b = ntohl(*b);
    return sizeof(*b);
}

int64 QoreSocketObject::recvi8(int timeout_ms, int64* b, ExceptionSink* xsink) {
    SimpleRefHolder<BinaryNode> data(qore_socket_object_exec_recv_bytes(this, sizeof(*b), timeout_ms, xsink));
    if (*xsink) {
        return -1;
    }
    memcpy(b, data->getPtr(), sizeof(*b));
    priv->socket->priv->do_data_event(QORE_EVENT_SOCKET_DATA_READ, QORE_SOURCE_SOCKET, **data);
    *b = MSBi8(*b);
    return sizeof(*b);
}

int64 QoreSocketObject::recvi2LSB(int timeout_ms, short* b, ExceptionSink* xsink) {
    SimpleRefHolder<BinaryNode> data(qore_socket_object_exec_recv_bytes(this, sizeof(*b), timeout_ms, xsink));
    if (*xsink) {
        return -1;
    }
    memcpy(b, data->getPtr(), sizeof(*b));
    priv->socket->priv->do_data_event(QORE_EVENT_SOCKET_DATA_READ, QORE_SOURCE_SOCKET, **data);
    *b = LSBi2(*b);
    return sizeof(*b);
}

int64 QoreSocketObject::recvi4LSB(int timeout_ms, int* b, ExceptionSink* xsink) {
    SimpleRefHolder<BinaryNode> data(qore_socket_object_exec_recv_bytes(this, sizeof(*b), timeout_ms, xsink));
    if (*xsink) {
        return -1;
    }
    memcpy(b, data->getPtr(), sizeof(*b));
    priv->socket->priv->do_data_event(QORE_EVENT_SOCKET_DATA_READ, QORE_SOURCE_SOCKET, **data);
    *b = LSBi4(*b);
    return sizeof(*b);
}

int64 QoreSocketObject::recvi8LSB(int timeout_ms, int64* b, ExceptionSink* xsink) {
    SimpleRefHolder<BinaryNode> data(qore_socket_object_exec_recv_bytes(this, sizeof(*b), timeout_ms, xsink));
    if (*xsink) {
        return -1;
    }
    memcpy(b, data->getPtr(), sizeof(*b));
    priv->socket->priv->do_data_event(QORE_EVENT_SOCKET_DATA_READ, QORE_SOURCE_SOCKET, **data);
    *b = LSBi8(*b);
    return sizeof(*b);
}

// receive integers and convert from network byte order
int64 QoreSocketObject::recvu1(int timeout_ms, unsigned char* b, ExceptionSink* xsink) {
    SimpleRefHolder<BinaryNode> data(qore_socket_object_exec_recv_bytes(this, sizeof(*b), timeout_ms, xsink));
    if (*xsink) {
        return -1;
    }
    memcpy(b, data->getPtr(), sizeof(*b));
    priv->socket->priv->do_data_event(QORE_EVENT_SOCKET_DATA_READ, QORE_SOURCE_SOCKET, **data);
    return sizeof(*b);
}

int64 QoreSocketObject::recvu2(int timeout_ms, unsigned short* b, ExceptionSink* xsink) {
    SimpleRefHolder<BinaryNode> data(qore_socket_object_exec_recv_bytes(this, sizeof(*b), timeout_ms, xsink));
    if (*xsink) {
        return -1;
    }
    memcpy(b, data->getPtr(), sizeof(*b));
    priv->socket->priv->do_data_event(QORE_EVENT_SOCKET_DATA_READ, QORE_SOURCE_SOCKET, **data);
    *b = ntohs(*b);
    return sizeof(*b);
}

int64 QoreSocketObject::recvu4(int timeout_ms, unsigned int* b, ExceptionSink* xsink) {
    SimpleRefHolder<BinaryNode> data(qore_socket_object_exec_recv_bytes(this, sizeof(*b), timeout_ms, xsink));
    if (*xsink) {
        return -1;
    }
    memcpy(b, data->getPtr(), sizeof(*b));
    priv->socket->priv->do_data_event(QORE_EVENT_SOCKET_DATA_READ, QORE_SOURCE_SOCKET, **data);
    *b = ntohl(*b);
    return sizeof(*b);
}

int64 QoreSocketObject::recvu2LSB(int timeout_ms, unsigned short* b, ExceptionSink* xsink) {
    SimpleRefHolder<BinaryNode> data(qore_socket_object_exec_recv_bytes(this, sizeof(*b), timeout_ms, xsink));
    if (*xsink) {
        return -1;
    }
    memcpy(b, data->getPtr(), sizeof(*b));
    priv->socket->priv->do_data_event(QORE_EVENT_SOCKET_DATA_READ, QORE_SOURCE_SOCKET, **data);
    *b = LSBi2(*b);
    return sizeof(*b);
}

int64 QoreSocketObject::recvu4LSB(int timeout_ms, unsigned int* b, ExceptionSink* xsink) {
    SimpleRefHolder<BinaryNode> data(qore_socket_object_exec_recv_bytes(this, sizeof(*b), timeout_ms, xsink));
    if (*xsink) {
        return -1;
    }
    memcpy(b, data->getPtr(), sizeof(*b));
    priv->socket->priv->do_data_event(QORE_EVENT_SOCKET_DATA_READ, QORE_SOURCE_SOCKET, **data);
    *b = LSBi4(*b);
    return sizeof(*b);
}

// send HTTP message
int QoreSocketObject::sendHTTPMessage(ExceptionSink* xsink, QoreHashNode* info, const char* method, const char* path,
        const char* http_version, const QoreHashNode* headers, const void* ptr, int size, int source,
        int timeout_ms) {
    return qore_socket_object_exec_send_http_message(this, info, method, path, http_version, headers, ptr, size,
        nullptr, source, timeout_ms, xsink);
}

int QoreSocketObject::sendHTTPMessage(ExceptionSink* xsink, QoreHashNode* info, const char* method, const char* path,
        const char* http_version, const QoreHashNode* headers, const QoreStringNode& body, int source,
        int timeout_ms) {
    QoreStringNodeValueHelper tstr(&body, priv->socket->getEncoding(), xsink);
    if (*xsink) {
        return -1;
    }
    return qore_socket_object_exec_send_http_message(this, info, method, path, http_version, headers, tstr->c_str(),
        tstr->size(), *tstr, source, timeout_ms, xsink);
}

int QoreSocketObject::sendHTTPMessageWithCallback(ExceptionSink* xsink, QoreHashNode* info, const char* method,
        const char* path, const char* http_version, const QoreHashNode* headers,
        const ResolvedCallReferenceNode& send_callback, int source, int timeout_ms, bool* aborted) {
    return qore_socket_object_exec_send_http_message_callback(this, info, method, path, http_version, headers,
        &send_callback, source, timeout_ms, aborted, xsink);
}

// send HTTP response
int QoreSocketObject::sendHTTPResponse(ExceptionSink* xsink, QoreHashNode* info, int code, const char* desc,
        const char* http_version, const QoreHashNode* headers, const void* ptr, size_t size, int source,
        int timeout_ms) {
    return qore_socket_object_exec_send_http_response(this, info, code, desc, http_version, headers, ptr, size,
        nullptr, source, timeout_ms, xsink);
}

int QoreSocketObject::sendHTTPResponse(ExceptionSink* xsink, QoreHashNode* info, int code, const char* desc,
        const char* http_version, const QoreHashNode* headers, const QoreStringNode& body, int source,
        int timeout_ms) {
    QoreStringNodeValueHelper tstr(&body, priv->socket->getEncoding(), xsink);
    if (*xsink) {
        return -1;
    }
    return qore_socket_object_exec_send_http_response(this, info, code, desc, http_version, headers, tstr->c_str(),
        tstr->size(), *tstr, source, timeout_ms, xsink);
}

int QoreSocketObject::sendHTTPResponse(ExceptionSink* xsink, QoreHashNode* info, int code, const char* desc,
        const char* http_version, const QoreHashNode* headers, InputStream *input_stream, size_t max_chunked_size,
        const ResolvedCallReferenceNode* trailer_callback, int source, int timeout_ms) {
    return qore_socket_object_exec_send_http_response_input_stream(this, info, code, desc, http_version, headers,
        input_stream, max_chunked_size, trailer_callback, source, timeout_ms, xsink);
}

int QoreSocketObject::sendHTTPResponseWithCallback(ExceptionSink* xsink, QoreHashNode* info, int code,
        const char* desc, const char* http_version, const QoreHashNode* headers,
    const ResolvedCallReferenceNode& send_callback, int source, int timeout_ms, bool* aborted) {
    return qore_socket_object_exec_send_http_response_callback(this, info, code, desc, http_version, headers,
        &send_callback, source, timeout_ms, aborted, xsink);
}

// send data in HTTP chunked format
void QoreSocketObject::sendHTTPChunkedBodyFromInputStream(InputStream* is, size_t max_chunked_size,
        const int timeout_ms, const ResolvedCallReferenceNode* trailer_callback, ExceptionSink* xsink) {
    qore_socket_object_exec_send_http_chunked_body_input_stream(this, is, max_chunked_size, trailer_callback,
        timeout_ms, xsink);
}

void QoreSocketObject::sendHTTPChunkedBodyTrailer(const QoreHashNode* headers, int timeout_ms, ExceptionSink* xsink) {
    qore_socket_object_exec_send_http_chunked_body_trailer(this, headers, QORE_SOURCE_SOCKET, timeout_ms, xsink);
}

QoreHashNode* QoreSocketObject::readHttpChunk(int timeout_ms, ExceptionSink* xsink) {
    return qore_socket_object_exec_read_http_chunked_body(this, timeout_ms, true, true, "readHTTPChunk", xsink);
}

// receive a binary message in HTTP chunked format
QoreHashNode* QoreSocketObject::readHTTPChunkedBodyBinary(int timeout_ms, ExceptionSink* xsink) {
    return qore_socket_object_exec_read_http_chunked_body(this, timeout_ms, true, false,
        "readHTTPChunkedBodyBinary", xsink);
}

// receive a binary message in HTTP chunked format
QoreHashNode* QoreSocketObject::readHTTPChunkedBodyToOutputStream(OutputStream* os, int timeout_ms, ExceptionSink* xsink) {
    return qore_socket_object_exec_read_http_chunked_body(this, timeout_ms, true, false,
        "readHTTPChunkedBodyToOutputStream", xsink, os);
}

// receive a string message in HTTP chunked format
QoreHashNode* QoreSocketObject::readHTTPChunkedBody(int timeout_ms, ExceptionSink* xsink) {
    return qore_socket_object_exec_read_http_chunked_body(this, timeout_ms, false, false,
        "readHTTPChunkedBody", xsink);
}

void QoreSocketObject::readHTTPChunkedBodyBinaryWithCallback(const ResolvedCallReferenceNode& recv_callback,
        QoreObject* obj, int timeout_ms, ExceptionSink* xsink) {
    qore_socket_object_exec_read_http_chunked_body(this, timeout_ms, true, false,
        "readHTTPChunkedBodyBinaryWithCallback", xsink, nullptr, &recv_callback, obj);
}

// receive a string message in HTTP chunked format
void QoreSocketObject::readHTTPChunkedBodyWithCallback(const ResolvedCallReferenceNode& recv_callback,
        QoreObject* obj, int timeout_ms, ExceptionSink* xsink) {
    qore_socket_object_exec_read_http_chunked_body(this, timeout_ms, false, false,
        "readHTTPChunkedBodyWithCallback", xsink, nullptr, &recv_callback, obj);
}

// read and parse HTTP header
AbstractQoreNode* QoreSocketObject::readHTTPHeader(ExceptionSink* xsink, QoreHashNode* info, int timeout_ms) {
    return qore_socket_object_exec_read_http_header(this, info, timeout_ms, xsink);
}

QoreStringNode* QoreSocketObject::readHTTPHeaderString(ExceptionSink* xsink, int timeout_ms) {
    return qore_socket_object_exec_read_http_header_string(this, timeout_ms, xsink);
}

QoreHashNode* QoreSocketObject::readServerSentEvent(ExceptionSink* xsink, const QoreStringNode* content_encoding,
        int timeout_ms) {
    return content_encoding
        ? qore_socket_object_exec_read_server_sent_event_encoded(this, content_encoding, timeout_ms, xsink)
        : qore_socket_object_exec_read_server_sent_event(this, timeout_ms, xsink);
}

int QoreSocketObject::setSendTimeout(int ms) {
    ExceptionSink xsink;
    int rc = qore_socket_object_exec_setup(this, new SocketSetupPollOperation(&xsink, this,
        SocketSetupPollOperation::ConfigAction::SetSendTimeout, ms), "setSendTimeout", "done", &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

int QoreSocketObject::setRecvTimeout(int ms) {
    ExceptionSink xsink;
    int rc = qore_socket_object_exec_setup(this, new SocketSetupPollOperation(&xsink, this,
        SocketSetupPollOperation::ConfigAction::SetRecvTimeout, ms), "setRecvTimeout", "done", &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

int QoreSocketObject::getSendTimeout() {
    ExceptionSink xsink;
    int rc = qore_socket_object_exec_setup(this, new SocketSetupPollOperation(&xsink, this,
        SocketSetupPollOperation::ConfigAction::GetSendTimeout), "getSendTimeout", "done", &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

int QoreSocketObject::getRecvTimeout() {
    ExceptionSink xsink;
    int rc = qore_socket_object_exec_setup(this, new SocketSetupPollOperation(&xsink, this,
        SocketSetupPollOperation::ConfigAction::GetRecvTimeout), "getRecvTimeout", "done", &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

int QoreSocketObject::close() {
    return qore_socket_object_exec_close(this);
}

int QoreSocketObject::shutdown() {
    ExceptionSink xsink;
    int rc = qore_socket_object_exec_shutdown(this, &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

int QoreSocketObject::shutdownSSL(ExceptionSink* xsink) {
    return qore_socket_object_exec_shutdown_ssl(this, xsink);
}

const char* QoreSocketObject::getSSLCipherName() {
    ExceptionSink xsink;
    SimpleRefHolder<QoreStringNode> str(getSSLCipherNameString(&xsink));
    AutoLocker al(priv->m);
    if (xsink || !str) {
        xsink.clear();
        priv->ssl_cipher_name_cache.clear();
        return nullptr;
    }
    priv->ssl_cipher_name_cache = str->c_str();
    return priv->ssl_cipher_name_cache.c_str();
}

const char* QoreSocketObject::getSSLCipherVersion() {
    ExceptionSink xsink;
    SimpleRefHolder<QoreStringNode> str(getSSLCipherVersionString(&xsink));
    AutoLocker al(priv->m);
    if (xsink || !str) {
        xsink.clear();
        priv->ssl_cipher_version_cache.clear();
        return nullptr;
    }
    priv->ssl_cipher_version_cache = str->c_str();
    return priv->ssl_cipher_version_cache.c_str();
}

bool QoreSocketObject::isSecure() {
    ExceptionSink xsink;
    bool rv = isSecure(&xsink);
    if (xsink) {
        xsink.clear();
        return false;
    }
    return rv;
}

QoreStringNode* QoreSocketObject::getSSLCipherNameString(ExceptionSink* xsink) {
    if (qore_on_async_io_thread()) {
        AutoLocker al(priv->m);
        const char* str = priv->socket->getSSLCipherName();
        return str ? new QoreStringNode(str) : nullptr;
    }

    ValueHolder rv(qore_socket_object_exec_poll_output(this,
        new QoreSocketObjectTlsStatePollOperation(this,
            QoreSocketObjectTlsStatePollOperation::Action::GetCipherName),
        -1, "getSSLCipherName", "tls-state", xsink), xsink);
    if (*xsink || rv->isNothing()) {
        return nullptr;
    }
    return rv->take<QoreStringNode>();
}

QoreStringNode* QoreSocketObject::getSSLCipherVersionString(ExceptionSink* xsink) {
    if (qore_on_async_io_thread()) {
        AutoLocker al(priv->m);
        const char* str = priv->socket->getSSLCipherVersion();
        return str ? new QoreStringNode(str) : nullptr;
    }

    ValueHolder rv(qore_socket_object_exec_poll_output(this,
        new QoreSocketObjectTlsStatePollOperation(this,
            QoreSocketObjectTlsStatePollOperation::Action::GetCipherVersion),
        -1, "getSSLCipherVersion", "tls-state", xsink), xsink);
    if (*xsink || rv->isNothing()) {
        return nullptr;
    }
    return rv->take<QoreStringNode>();
}

bool QoreSocketObject::isSecure(ExceptionSink* xsink) {
    if (qore_on_async_io_thread()) {
        return isSecureForAsyncPoll();
    }

    ValueHolder rv(qore_socket_object_exec_poll_output(this,
        new QoreSocketObjectTlsStatePollOperation(this,
            QoreSocketObjectTlsStatePollOperation::Action::IsSecure),
        -1, "isSecure", "tls-state", xsink), xsink);
    return *xsink ? false : rv->getAsBool();
}

bool QoreSocketObject::isSecureForAsyncPoll() const {
    AutoLocker al(priv->m);
    return priv->socket->isSecure();
}

int QoreSocketObject::checkIdleData(ExceptionSink* xsink) {
    return qore_socket_object_exec_check_idle_data(this, xsink);
}

int QoreSocketObject::checkIdleDataForAsyncPoll(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    return checkIdleDataForAsyncPollLocked(xsink);
}

int QoreSocketObject::checkIdleDataForAsyncPollLocked(ExceptionSink* xsink) {
    assert(priv->m.trylock());

    qore_socket_private* p = qore_socket_private::get(*priv->socket);
    if (!p->isOpen()) {
        return -1;
    }
    if (p->ssl) {
        // TLS connection: use SSL_peek to drain TLS post-handshake records
        // (for example TLS 1.3 NewSessionTicket). SSL_peek consumes such
        // records from the TCP socket into OpenSSL's internal buffer and
        // reports only application data availability.
        bool ready = p->peekSslApplicationData("checkIdleData", xsink);
        if (*xsink) {
            return -1;
        }
        if (!p->isOpen()) {
            return -1;
        }
        return ready ? 1 : 0;
    }
    // Plain TCP: raw non-blocking peek. Callers are async poll operations
    // already running under AsyncIoController ownership.
    char peek_buf;
    ssize_t rc = ::recv(p->sock, &peek_buf, 1, MSG_PEEK | MSG_DONTWAIT);
    if (rc > 0) {
        return 1;
    }
    if (rc == 0) {
        return -1;
    }
    int e = sock_get_error();
    if (e == EAGAIN
#ifdef EWOULDBLOCK
            || e == EWOULDBLOCK
#endif
            || e == EINTR) {
        return 0;
    }
    // Fatal idle-probe errors (for example ECONNRESET) mean the connection
    // is no longer usable.  Idle monitors close the socket without surfacing
    // a noisy request error.
    return -1;
}

void QoreSocketObject::setAlpnProtocols(const QoreListNode* protocols, ExceptionSink* xsink) {
    if (qore_on_async_io_thread() || qore_in_async_io_continue_poll_worker()) {
        setAlpnProtocolsForAsyncPoll(protocols, xsink);
        return;
    }

    std::vector<std::string> proto_list;
    if (my_socket_priv::parseAlpnProtocols(protocols, proto_list, xsink)) {
        return;
    }
    ValueHolder rv(qore_socket_object_exec_poll_output(this,
        new QoreSocketObjectTlsStatePollOperation(this, std::move(proto_list)),
        -1, "setAlpnProtocols", "tls-state", xsink), xsink);
}

void QoreSocketObject::setAlpnProtocolsForAsyncPoll(const QoreListNode* protocols, ExceptionSink* xsink) {
    std::vector<std::string> proto_list;
    if (my_socket_priv::parseAlpnProtocols(protocols, proto_list, xsink)) {
        return;
    }
    AutoLocker al(priv->m);
    qore_socket_private::get(*priv->socket)->alpn_protocols = std::move(proto_list);
}

QoreStringNode* QoreSocketObject::getAlpnProtocol() const {
    ExceptionSink xsink;
    QoreStringNode* rv = getAlpnProtocol(&xsink);
    if (xsink) {
        xsink.clear();
        return nullptr;
    }
    return rv;
}

QoreStringNode* QoreSocketObject::getAlpnProtocol(ExceptionSink* xsink) const {
    if (qore_on_async_io_thread()) {
        return getAlpnProtocolForAsyncPoll();
    }

    ValueHolder rv(qore_socket_object_exec_poll_output(const_cast<QoreSocketObject*>(this),
        new QoreSocketObjectTlsStatePollOperation(const_cast<QoreSocketObject*>(this),
            QoreSocketObjectTlsStatePollOperation::Action::GetAlpnProtocol),
        -1, "getAlpnProtocol", "tls-state", xsink), xsink);
    if (*xsink || rv->isNothing()) {
        return nullptr;
    }
    return rv->take<QoreStringNode>();
}

QoreStringNode* QoreSocketObject::getAlpnProtocolForAsyncPoll() const {
    AutoLocker al(priv->m);
    return priv->socket->getAlpnProtocol();
}

bool QoreSocketObject::isHttp2() const {
    ExceptionSink xsink;
    bool rv = isHttp2(&xsink);
    if (xsink) {
        xsink.clear();
        return false;
    }
    return rv;
}

bool QoreSocketObject::isHttp2(ExceptionSink* xsink) const {
    if (qore_on_async_io_thread()) {
        AutoLocker al(priv->m);
        return priv->socket->isHttp2();
    }

    ValueHolder rv(qore_socket_object_exec_poll_output(const_cast<QoreSocketObject*>(this),
        new QoreSocketObjectTlsStatePollOperation(const_cast<QoreSocketObject*>(this),
            QoreSocketObjectTlsStatePollOperation::Action::IsHttp2),
        -1, "isHttp2", "tls-state", xsink), xsink);
    return *xsink ? false : rv->getAsBool();
}

int32_t QoreSocketObject::submitHttp2PushPromise(int32_t stream_id, const char* path,
        const QoreHashNode* headers, ExceptionSink* xsink) {
    return static_cast<int32_t>(qore_socket_object_exec_http2_enqueue_int(this,
        new QoreSocketObjectHttp2EnqueuePollOperation(this,
            QoreSocketObjectHttp2EnqueuePollOperation::Action::PushPromise, stream_id, path, headers),
        "submitHttp2PushPromise", xsink));
}

int QoreSocketObject::submitHttp2Response(int32_t stream_id, int status_code,
        const QoreHashNode* headers, const void* body, size_t body_len,
        ExceptionSink* xsink) {
    return static_cast<int>(qore_socket_object_exec_http2_enqueue_int(this,
        new QoreSocketObjectHttp2EnqueuePollOperation(this,
            QoreSocketObjectHttp2EnqueuePollOperation::Action::Response, stream_id, status_code, headers, body,
            body_len),
        "submitHttp2Response", xsink));
}

int QoreSocketObject::submitHttp2ConnectResponse(int32_t stream_id, int status_code,
        const QoreHashNode* headers, ExceptionSink* xsink) {
    return static_cast<int>(qore_socket_object_exec_http2_enqueue_int(this,
        new QoreSocketObjectHttp2EnqueuePollOperation(this,
            QoreSocketObjectHttp2EnqueuePollOperation::Action::ConnectResponse, stream_id, status_code, headers),
        "submitHttp2ConnectResponse", xsink));
}

static int32_t qore_socket_object_submit_http2_request(QoreSocketObject* s, const QoreHashNode* headers,
        const void* body, size_t body_len, ExceptionSink* xsink, bool streaming, bool wake_controller) {
    // Fast path: if the caller is already running on the async I/O
    // controller's I/O thread or in a controller-owned continuePoll worker,
    // dispatching through the synchronous controller-backed Socket API would
    // either deadlock or create a nested poll wait.  The underlying
    // h2->submitRequest() is only a non-blocking nghttp2 enqueue (no socket I/O
    // happens here), so direct submission is the correct async path.
    bool on_io_thread = qore_on_async_io_thread();
    if (on_io_thread || qore_in_async_io_continue_poll_worker()) {
        std::string method;
        std::string path;
        std::vector<std::pair<std::string, std::string>> request_headers;
        if (qore_socket_object_parse_http2_request_headers(headers, method, path, request_headers, xsink)) {
            return -1;
        }
        Http2SessionPtr h2 = qore_socket_object_get_h2_session(s);
        if (!h2) {
            xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
            return -1;
        }
        int32_t stream_id = h2->submitRequest(method.c_str(), path.c_str(), request_headers,
            body, body_len, xsink, streaming);
        if (*xsink || stream_id < 0) {
            return stream_id;
        }
        if (wake_controller && !on_io_thread) {
            qore_socket_object_wake_async_controller(s, xsink);
            if (*xsink) {
                return -1;
            }
        }
        return stream_id;
    }

    QoreSocketObjectHttp2EnqueuePollOperation* poller =
        new QoreSocketObjectHttp2EnqueuePollOperation(s, headers, body, body_len, streaming, wake_controller, xsink);
    if (*xsink) {
        poller->deref(xsink);
        return -1;
    }
    return static_cast<int32_t>(qore_socket_object_exec_http2_enqueue_int(s, poller, "submitHttp2Request",
        xsink));
}

int32_t QoreSocketObject::submitHttp2Request(const QoreHashNode* headers, const void* body,
        size_t body_len, ExceptionSink* xsink, bool streaming) {
    return qore_socket_object_submit_http2_request(this, headers, body, body_len, xsink, streaming, true);
}

int32_t QoreSocketObject::submitHttp2RequestNoWake(const QoreHashNode* headers, const void* body,
        size_t body_len, ExceptionSink* xsink, bool streaming) {
    return qore_socket_object_submit_http2_request(this, headers, body, body_len, xsink, streaming, false);
}

void QoreSocketObject::cancelHttp2Stream(int32_t stream_id, ExceptionSink* xsink) {
    qore_socket_object_exec_http2_enqueue_int(this,
        new QoreSocketObjectHttp2EnqueuePollOperation(this,
            QoreSocketObjectHttp2EnqueuePollOperation::Action::Cancel, stream_id),
        "cancelHttp2Stream", xsink);
}

void QoreSocketObject::setHttp2StreamStreaming(int32_t stream_id) {
    ExceptionSink xsink;
    setHttp2StreamStreaming(stream_id, &xsink);
    if (xsink) {
        xsink.clear();
    }
}

int QoreSocketObject::setHttp2StreamStreaming(int32_t stream_id, ExceptionSink* xsink) {
    if (!qore_on_async_io_thread()) {
        return static_cast<int>(qore_socket_object_exec_http2_enqueue_int(this,
            new QoreSocketObjectHttp2EnqueuePollOperation(this,
                QoreSocketObjectHttp2EnqueuePollOperation::Action::SetStreaming, stream_id, false),
            "setHttp2StreamStreaming", xsink));
    }

    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    if (h2) {
        h2->setStreamStreaming(stream_id);
    }
    return 0;
}

void QoreSocketObject::setHttp2StreamStreamingDirect(int32_t stream_id) {
    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    if (h2) {
        h2->setStreamStreaming(stream_id);
    }
}

void QoreSocketObject::setHttp2ConnectProtocolEnabled(bool enable) {
    AutoLocker al(priv->m);
    priv->socket->setHttp2ConnectProtocolEnabled(enable);
}

int QoreSocketObject::sendHttp2StreamData(int32_t stream_id, const BinaryNode* data,
        bool end_stream, ExceptionSink* xsink) {
    return static_cast<int>(qore_socket_object_exec_http2_enqueue_int(this,
        new QoreSocketObjectHttp2EnqueuePollOperation(this, stream_id, data, end_stream),
        "sendHttp2StreamData", xsink));
}

int QoreSocketObject::sendHttp2StreamDataForAsyncPoll(int32_t stream_id, const BinaryNode* data,
        bool end_stream, ExceptionSink* xsink) {
    if (!qore_on_async_io_thread()) {
        return sendHttp2StreamData(stream_id, data, end_stream, xsink);
    }

    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    if (!h2) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
        return -1;
    }

    const void* ptr = data ? data->getPtr() : nullptr;
    size_t len = data ? data->size() : 0;
    int rv = h2->sendStreamData(stream_id, ptr, len, end_stream, xsink);
    if (rv < 0) {
        return -1;
    }
    if (rv > 0) {
        xsink->raiseException("HTTP2-FLOW-CONTROL", "stream %d buffer full: data dropped", stream_id);
        return -1;
    }
    return 0;
}

BinaryNode* QoreSocketObject::readHttp2StreamData(int32_t stream_id, size_t max_bytes, ExceptionSink* xsink) {
    return qore_socket_object_exec_http2_stream_data(this,
        new QoreSocketObjectHttp2StreamDataPollOperation(this, stream_id, max_bytes), stream_id, xsink);
}

BinaryNode* QoreSocketObject::readHttp2StreamDataForAsyncPoll(int32_t stream_id, size_t max_bytes,
        ExceptionSink* xsink) {
    if (!qore_on_async_io_thread()) {
        return readHttp2StreamData(stream_id, max_bytes, xsink);
    }

    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    if (!h2) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
        return nullptr;
    }
    return h2->takeStreamData(stream_id, max_bytes, xsink);
}

int QoreSocketObject::sendHttp2Trailers(int32_t stream_id, const QoreHashNode* trailers,
        ExceptionSink* xsink) {
    return static_cast<int>(qore_socket_object_exec_http2_enqueue_int(this,
        new QoreSocketObjectHttp2EnqueuePollOperation(this,
            QoreSocketObjectHttp2EnqueuePollOperation::Action::Trailers, stream_id, 0, trailers),
        "sendHttp2Trailers", xsink));
}

int QoreSocketObject::flushHttp2PendingData(ExceptionSink* xsink) {
    SocketSyncPoll::assertNotOnIoThread("Socket", "flushHttp2PendingData", xsink);
    if (*xsink) {
        return -1;
    }
    return qore_socket_object_exec_http2_flush(this, "flushHttp2PendingData", xsink, false, true);
}

int QoreSocketObject::flushHttp2PendingDataForAsyncPoll(ExceptionSink* xsink) {
    if (!qore_on_async_io_thread()) {
        return flushHttp2PendingData(xsink);
    }

    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    return h2 ? h2->sendPendingData(0, xsink) : 0;
}

int QoreSocketObject::submitHttp2Ping(ExceptionSink* xsink) {
    SocketSyncPoll::assertNotOnIoThread("Socket", "submitHttp2Ping", xsink);
    if (*xsink) {
        return -1;
    }
    return qore_socket_object_exec_http2_flush(this, "submitHttp2Ping", xsink, true, true);
}

int QoreSocketObject::submitHttp2PingForAsyncPoll(ExceptionSink* xsink) {
    if (!qore_on_async_io_thread()) {
        return submitHttp2Ping(xsink);
    }

    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    if (!h2) {
        return 0;
    }

    int rv = h2->submitPing(nullptr, xsink);
    if (rv < 0 || *xsink) {
        return -1;
    }
    return h2->sendPendingData(0, xsink);
}

// Async-path H2 server write methods. Non-I/O-thread callers delegate session
// mutation to the async I/O controller; callbacks already running inside the
// owning poll operation use explicit ForAsyncPoll helpers instead of recursing
// through the blocking controller path.
int QoreSocketObject::submitHttp2StreamingResponseHeadersAsync(int32_t stream_id, int status_code,
        const QoreHashNode* headers, ExceptionSink* xsink) {
    return static_cast<int>(qore_socket_object_exec_http2_enqueue_int(this,
        new QoreSocketObjectHttp2EnqueuePollOperation(this,
            QoreSocketObjectHttp2EnqueuePollOperation::Action::StreamingResponseHeaders, stream_id, status_code,
            headers),
        "submitHttp2StreamingResponseHeaders", xsink));
}

int QoreSocketObject::sendHttp2StreamDataAsync(int32_t stream_id, const BinaryNode* data,
        bool end_stream, ExceptionSink* xsink) {
    return static_cast<int>(qore_socket_object_exec_http2_enqueue_int(this,
        new QoreSocketObjectHttp2EnqueuePollOperation(this, stream_id, data, end_stream),
        "sendHttp2StreamData", xsink));
}

int QoreSocketObject::sendHttp2TrailersAsync(int32_t stream_id, const QoreHashNode* trailers,
        ExceptionSink* xsink) {
    return static_cast<int>(qore_socket_object_exec_http2_enqueue_int(this,
        new QoreSocketObjectHttp2EnqueuePollOperation(this,
            QoreSocketObjectHttp2EnqueuePollOperation::Action::Trailers, stream_id, 0, trailers),
        "sendHttp2Trailers", xsink));
}

void QoreSocketObject::cleanupHttp2StreamAsync(int32_t stream_id) {
    if (qore_on_async_io_thread()) {
        Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
        if (h2) {
            h2->cleanupStream(stream_id);
        }
        return;
    }

    ExceptionSink xsink;
    qore_socket_object_exec_http2_enqueue_int(this,
        new QoreSocketObjectHttp2EnqueuePollOperation(this,
            QoreSocketObjectHttp2EnqueuePollOperation::Action::Cleanup, stream_id, false),
        "cleanupHttp2Stream", &xsink);
    if (xsink) {
        xsink.clear();
    }
}

int QoreSocketObject::resetHttp2StreamAsync(int32_t stream_id, ExceptionSink* xsink) {
    return resetHttp2StreamAsync(stream_id, NGHTTP2_CANCEL, xsink);
}

int QoreSocketObject::resetHttp2StreamAsync(int32_t stream_id, uint32_t error_code, ExceptionSink* xsink) {
    if (qore_on_async_io_thread()) {
        Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
        if (!h2) {
            return 0;
        }
        int rv = h2->submitRstStream(stream_id, error_code, xsink);
        if (rv != 0) {
            printd(2, "resetHttp2StreamAsync() submitRstStream failed for stream %d (rv=%d), "
                "cleaning up local state anyway\n", stream_id, rv);
        }
        if (!rv && !*xsink) {
            (void)h2->sendPendingData(0, xsink);
        }
        h2->cleanupStream(stream_id);
        return rv;
    }

    return static_cast<int>(qore_socket_object_exec_http2_enqueue_int(this,
        new QoreSocketObjectHttp2EnqueuePollOperation(this,
            QoreSocketObjectHttp2EnqueuePollOperation::Action::Reset, stream_id, error_code),
        "resetHttp2Stream", xsink));
}

int QoreSocketObject::submitHttp2StreamingResponseWithStream(int32_t stream_id, int status_code,
        const QoreHashNode* headers, InputStream* body, ExceptionSink* xsink) {
    // C++ vtable is the sole authority on I/O thread eligibility.
    // Return 1 (not accepted) so the caller can fall back to handler-thread streaming.
    // No exception — the caller handles the fallback path.
    if (!body->isIoThreadSafe()) {
        return 1;
    }

    // Transfer ownership of InputStream to the session for I/O thread reading
    // The handler thread unassigns before calling; I/O thread will reassign on first read
    body->unassignThread(xsink);
    if (*xsink) {
        return -1;
    }

    return static_cast<int>(qore_socket_object_exec_http2_enqueue_int(this,
        new QoreSocketObjectHttp2EnqueuePollOperation(this, stream_id, status_code, headers, body,
            qore_socket_object_get_content_length(headers)),
        "submitHttp2StreamingResponseWithStream", xsink));
}

bool QoreSocketObject::isHttp2StreamClosed(int32_t stream_id) const {
    ExceptionSink xsink;
    bool rv = qore_socket_object_exec_http2_stream_state(const_cast<QoreSocketObject*>(this),
        new QoreSocketObjectHttp2StreamStatePollOperation(const_cast<QoreSocketObject*>(this), stream_id,
            QoreSocketObjectHttp2StreamStatePollOperation::Action::Closed),
        stream_id, "isHttp2StreamClosed", &xsink);
    if (xsink) {
        xsink.clear();
        return true;
    }
    return rv;
}

bool QoreSocketObject::isHttp2StreamClosedForAsyncPoll(int32_t stream_id) const {
    if (!qore_on_async_io_thread()) {
        return isHttp2StreamClosed(stream_id);
    }

    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    return !h2 || h2->isStreamClosed(stream_id);
}

bool QoreSocketObject::isHttp2StreamRemoteClosed(int32_t stream_id) const {
    ExceptionSink xsink;
    bool rv = qore_socket_object_exec_http2_stream_state(const_cast<QoreSocketObject*>(this),
        new QoreSocketObjectHttp2StreamStatePollOperation(const_cast<QoreSocketObject*>(this), stream_id,
            QoreSocketObjectHttp2StreamStatePollOperation::Action::RemoteClosed),
        stream_id, "isHttp2StreamRemoteClosed", &xsink);
    if (xsink) {
        xsink.clear();
        return true;
    }
    return rv;
}

bool QoreSocketObject::isHttp2StreamRemoteClosedForAsyncPoll(int32_t stream_id) const {
    if (!qore_on_async_io_thread()) {
        return isHttp2StreamRemoteClosed(stream_id);
    }

    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    return !h2 || h2->isStreamRemoteClosed(stream_id);
}

std::vector<int32_t> QoreSocketObject::takeHttp2PeerResetReportsForAsyncPoll() {
    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    if (!h2) {
        return {};
    }
    return h2->takePendingPeerResetReports();
}

int QoreSocketObject::waitForHttp2StreamDrain(int32_t stream_id, int timeout_ms) {
    ExceptionSink xsink;
    int rc = waitForHttp2StreamDrain(stream_id, timeout_ms, &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

int QoreSocketObject::waitForHttp2StreamDrain(int32_t stream_id, int timeout_ms, ExceptionSink* xsink) {
    SocketSyncPoll::assertNotOnIoThread("Socket", "waitForHttp2StreamDrain", xsink);
    if (*xsink) {
        return -1;
    }
    std::string owner_name("waitForHttp2StreamDrain:");
    owner_name += std::to_string(stream_id);
    return qore_socket_object_exec_stream_drain(this,
        new QoreSocketObjectStreamDrainPollOperation(this, stream_id, true), timeout_ms, owner_name.c_str(),
        xsink);
}

long QoreSocketObject::verifyPeerCertificate() {
    ExceptionSink xsink;
    long rv = verifyPeerCertificate(&xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rv;
}

long QoreSocketObject::verifyPeerCertificate(ExceptionSink* xsink) {
    if (qore_on_async_io_thread()) {
        AutoLocker al(priv->m);
        return priv->socket->verifyPeerCertificate();
    }

    ValueHolder rv(qore_socket_object_exec_poll_output(this,
        new QoreSocketObjectTlsStatePollOperation(this,
            QoreSocketObjectTlsStatePollOperation::Action::VerifyPeerCertificate),
        -1, "verifyPeerCertificate", "tls-state", xsink), xsink);
    return *xsink ? -1 : static_cast<long>(rv->getAsBigInt());
}

int QoreSocketObject::getPollableDescriptor() const {
    return priv->socket->getSocket();
}

#ifdef DARWIN
void QoreSocketObject::setPollNotifyFd(int fd) {
    qore_socket_private::get(*priv->socket)->poll_notify_fd.store(fd, std::memory_order_release);
}
#endif

int QoreSocketObject::getSocket() {
    return priv->socket->getSocket();
}

#ifdef DEBUG
void QoreSocketObject::dbgForceFdSwapNextWait() {
    AutoLocker al(priv->m);
    qore_socket_private::get(*priv->socket)->debug_force_fd_swap_next_wait = true;
}
#endif

void QoreSocketObject::setEncoding(const QoreEncoding* id) {
    priv->socket->setEncoding(id);
}

const QoreEncoding* QoreSocketObject::getEncoding() const {
    return priv->socket->getEncoding();
}

bool QoreSocketObject::isDataAvailable(ExceptionSink* xsink, int timeout_ms) {
    return qore_socket_object_exec_is_data_available(this, timeout_ms, xsink);
}

bool QoreSocketObject::isWriteFinished(ExceptionSink* xsink, int timeout_ms) {
    return qore_socket_object_exec_wait_readiness(this, timeout_ms, NB_SEND, SOCK_POLLOUT, "isWriteFinished",
        "waiting-write", "write-ready", xsink);
}

bool QoreSocketObject::isOpen() const {
    return priv->socket->isOpen();
}

int QoreSocketObject::setNoDelayForAsyncPoll(int nodelay, ExceptionSink* xsink) {
    if (!qore_on_async_io_thread()) {
        return setNoDelay(nodelay);
    }

    this->ref();
    ReferenceHolder<SocketSetupPollOperation> poller(new SocketSetupPollOperation(xsink, this,
        SocketSetupPollOperation::ConfigAction::SetNoDelay, nodelay), xsink);
    if (*xsink) {
        return -1;
    }

    poller->continuePoll(xsink);
    return *xsink ? -1 : poller->getRc();
}

int QoreSocketObject::connectINETSSL(ExceptionSink* xsink, const char* host, int port, int timeout_ms) {
    QoreString service;
    service.sprintf("%d", port);
    return qore_socket_object_exec_connect_inet(this, host, service.c_str(), AF_UNSPEC, SOCK_STREAM, 0, timeout_ms,
        true, xsink);
}

int QoreSocketObject::connectINET2SSL(ExceptionSink* xsink, const char* name, const char* service, int family,
        int sock_type, int protocol, int timeout_ms) {
    return qore_socket_object_exec_connect_inet(this, name, service, family, sock_type, protocol, timeout_ms, true,
        xsink);
}

int QoreSocketObject::connectUNIXSSL(ExceptionSink* xsink, const char* p, int sock_type, int protocol) {
    return qore_socket_object_exec_connect_unix(this, p, sock_type, protocol, -1, true, xsink);
}

int QoreSocketObject::connectSSL(ExceptionSink* xsink, const char* name, int timeout_ms) {
    return qore_socket_object_exec_connect(this, name, timeout_ms, true, xsink);
}

QoreSocketObject* QoreSocketObject::accept(SocketSource* source, ExceptionSink* xsink) {
    return qore_socket_object_exec_accept(this, -1, false, xsink, source);
}

QoreSocketObject* QoreSocketObject::acceptSSL(ExceptionSink* xsink, SocketSource* source) {
    return qore_socket_object_exec_accept(this, -1, true, xsink, source);
}

QoreSocketObject* QoreSocketObject::accept(int timeout_ms, ExceptionSink* xsink) {
    return qore_socket_object_exec_accept(this, timeout_ms, false, xsink);
}

QoreSocketObject* QoreSocketObject::acceptSSL(ExceptionSink* xsink, int timeout_ms) {
    return qore_socket_object_exec_accept(this, timeout_ms, true, xsink);
}

// c must be already referenced before this call
void QoreSocketObject::setCertificate(QoreSSLCertificate* c) {
    AutoLocker al(priv->m);
    if (priv->cert)
        priv->cert->deref();
    priv->cert = c;
}

// p must be already referenced before this call
void QoreSocketObject::setPrivateKey(QoreSSLPrivateKey* p) {
    AutoLocker al(priv->m);
    if (priv->pk)
        priv->pk->deref();
    priv->pk = p;
}

void QoreSocketObject::setCertificateAndPrivateKey(QoreSSLCertificate* c, QoreSSLPrivateKey* p) {
    AutoLocker al(priv->m);
    if (priv->cert) {
        priv->cert->deref();
    }
    priv->cert = c;
    if (priv->pk) {
        priv->pk->deref();
    }
    priv->pk = p;
}

void QoreSocketObject::freeQuicServerSslCtx() {
    // Uses the socket's dedicated quic_server_ssl_ctx_lock_ — does not take priv->m
    priv->socket->priv->freeQuicServerSslCtx();
}

void QoreSocketObject::upgradeClientToSSL(ExceptionSink* xsink, int timeout_ms) {
    qore_socket_object_exec_upgrade_ssl(this, timeout_ms, false, xsink);
}

void QoreSocketObject::upgradeServerToSSL(ExceptionSink* xsink, int timeout_ms) {
    qore_socket_object_exec_upgrade_ssl(this, timeout_ms, true, xsink);
}

void QoreSocketObject::setEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
    AutoLocker al(priv->m);
    priv->socket->setEventQueue(xsink, q, arg, with_data);
}

void QoreSocketObject::setEventQueue(Queue* cbq, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    priv->socket->setEventQueue(xsink, cbq, QoreValue(), false);
}

int QoreSocketObject::setNoDelay(int nodelay) {
    ExceptionSink xsink;
    int rc = qore_socket_object_exec_setup(this, new SocketSetupPollOperation(&xsink, this,
        SocketSetupPollOperation::ConfigAction::SetNoDelay, nodelay), "setNoDelay", "done", &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

int QoreSocketObject::getNoDelay() {
    ExceptionSink xsink;
    int rc = qore_socket_object_exec_setup(this, new SocketSetupPollOperation(&xsink, this,
        SocketSetupPollOperation::ConfigAction::GetNoDelay), "getNoDelay", "done", &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

int QoreSocketObject::setUserTimeout(int ms) {
    ExceptionSink xsink;
    int rc = qore_socket_object_exec_setup(this, new SocketSetupPollOperation(&xsink, this,
        SocketSetupPollOperation::ConfigAction::SetUserTimeout, ms), "setUserTimeout", "done", &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

int QoreSocketObject::getUserTimeout() {
    ExceptionSink xsink;
    int rc = qore_socket_object_exec_setup(this, new SocketSetupPollOperation(&xsink, this,
        SocketSetupPollOperation::ConfigAction::GetUserTimeout), "getUserTimeout", "done", &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

QoreHashNode* QoreSocketObject::getPeerInfo(ExceptionSink* xsink, bool host_lookup) const {
    return qore_socket_object_exec_address_info(const_cast<QoreSocketObject*>(this),
        QoreSocketObjectAddressInfoPollOperation::Action::Peer, host_lookup, "getPeerInfo",
        "SOCKET-GETPEERINFO-ERROR", xsink);
}

QoreHashNode* QoreSocketObject::getSocketInfo(ExceptionSink* xsink, bool host_lookup) const {
    return qore_socket_object_exec_address_info(const_cast<QoreSocketObject*>(this),
        QoreSocketObjectAddressInfoPollOperation::Action::Socket, host_lookup, "getSocketInfo",
        "SOCKET-GETSOCKETINFO-ERROR", xsink);
}

void QoreSocketObject::clearWarningQueue(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    priv->socket->clearWarningQueue(xsink);
}

void QoreSocketObject::setWarningQueue(ExceptionSink* xsink, int64 warning_ms, int64 warning_bs, Queue* wq,
        QoreValue arg, int64 min_ms) {
    AutoLocker al(priv->m);
    priv->socket->setWarningQueue(xsink, warning_ms, warning_bs, wq, arg, min_ms);
}

QoreHashNode* QoreSocketObject::getUsageInfo() const {
    AutoLocker al(priv->m);
    return priv->socket->getUsageInfo();
}

void QoreSocketObject::clearStats() {
    AutoLocker al(priv->m);
    priv->socket->clearStats();
}

bool QoreSocketObject::pendingHttpChunkedBody() const {
    AutoLocker al(priv->m);
    return priv->socket->pendingHttpChunkedBody();
}

void QoreSocketObject::setSslVerifyMode(int mode) {
    if (qore_on_async_io_thread() || qore_in_async_io_continue_poll_worker()) {
        ExceptionSink xsink;
        AutoLocker al(priv->m);
        if (!priv->checkValid(&xsink)) {
            qore_socket_private::get(*priv->socket)->setSslVerifyMode(mode);
        }
        if (xsink) {
            xsink.clear();
        }
        return;
    }

    ExceptionSink xsink;
    qore_socket_object_exec_setup(this, new SocketSetupPollOperation(&xsink, this,
        SocketSetupPollOperation::ConfigAction::SetSslVerifyMode, mode), "setSslVerifyMode", "done", &xsink);
    if (xsink) {
        xsink.clear();
    }
}

int QoreSocketObject::getSslVerifyMode() const {
    ExceptionSink xsink;
    int rc = qore_socket_object_exec_setup(const_cast<QoreSocketObject*>(this),
        new SocketSetupPollOperation(&xsink, const_cast<QoreSocketObject*>(this),
            SocketSetupPollOperation::ConfigAction::GetSslVerifyMode), "getSslVerifyMode", "done", &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

void QoreSocketObject::acceptAllCertificates(bool accept_all) {
    if (qore_on_async_io_thread() || qore_in_async_io_continue_poll_worker()) {
        ExceptionSink xsink;
        AutoLocker al(priv->m);
        if (!priv->checkValid(&xsink)) {
            qore_socket_private::get(*priv->socket)->acceptAllCertificates(accept_all);
        }
        if (xsink) {
            xsink.clear();
        }
        return;
    }

    ExceptionSink xsink;
    qore_socket_object_exec_setup(this, new SocketSetupPollOperation(&xsink, this,
        SocketSetupPollOperation::ConfigAction::SetAcceptAllCertificates, accept_all), "acceptAllCertificates",
        "done", &xsink);
    if (xsink) {
        xsink.clear();
    }
}

bool QoreSocketObject::getAcceptAllCertificates() const {
    ExceptionSink xsink;
    int rc = qore_socket_object_exec_setup(const_cast<QoreSocketObject*>(this),
        new SocketSetupPollOperation(&xsink, const_cast<QoreSocketObject*>(this),
            SocketSetupPollOperation::ConfigAction::GetAcceptAllCertificates), "getAcceptAllCertificates", "done",
        &xsink);
    if (xsink) {
        xsink.clear();
        return false;
    }
    return rc > 0;
}

bool QoreSocketObject::captureRemoteCertificates(bool set) {
    ExceptionSink xsink;
    int rc = qore_socket_object_exec_setup(this, new SocketSetupPollOperation(&xsink, this,
        SocketSetupPollOperation::ConfigAction::CaptureRemoteCertificates, set), "captureRemoteCertificates",
        "done", &xsink);
    if (xsink) {
        xsink.clear();
        return false;
    }
    return rc > 0;
}

QoreObject* QoreSocketObject::getRemoteCertificate() const {
    ExceptionSink xsink;
    QoreObject* rv = getRemoteCertificate(&xsink);
    if (xsink) {
        xsink.clear();
        return nullptr;
    }
    return rv;
}

QoreObject* QoreSocketObject::getRemoteCertificate(ExceptionSink* xsink) const {
    if (qore_on_async_io_thread()) {
        AutoLocker al(priv->m);
        return priv->socket->getRemoteCertificate();
    }

    ValueHolder rv(qore_socket_object_exec_poll_output(const_cast<QoreSocketObject*>(this),
        new QoreSocketObjectTlsStatePollOperation(const_cast<QoreSocketObject*>(this),
            QoreSocketObjectTlsStatePollOperation::Action::GetRemoteCertificate),
        -1, "getRemoteCertificate", "tls-state", xsink), xsink);
    if (*xsink || rv->isNothing()) {
        return nullptr;
    }
    return rv->take<QoreObject>();
}

int64 QoreSocketObject::getConnectionId() const {
    AutoLocker al(priv->m);
    return priv->socket->getConnectionId();
}

void QoreSocketObject::setMaxChunkedBodySize(int64 size) {
    ExceptionSink xsink;
    qore_socket_object_exec_setup(this, new SocketSetupPollOperation(&xsink, this,
        SocketSetupPollOperation::ConfigAction::SetMaxChunkedBodySize, size), "setMaxChunkedBodySize",
        "done", &xsink);
    if (xsink) {
        xsink.clear();
    }
}

int64 QoreSocketObject::getMaxChunkedBodySize() const {
    AutoLocker al(priv->m);
    return priv->socket->getMaxChunkedBodySize();
}

void QoreSocketObject::setHttp2MaxRequestBodySize(int64 size) {
    ExceptionSink xsink;
    qore_socket_object_exec_setup(this, new SocketSetupPollOperation(&xsink, this,
        SocketSetupPollOperation::ConfigAction::SetHttp2MaxRequestBodySize, size), "setHttp2MaxRequestBodySize",
        "done", &xsink);
    if (xsink) {
        xsink.clear();
    }
}

int64 QoreSocketObject::getHttp2MaxRequestBodySize() const {
    AutoLocker al(priv->m);
    return priv->socket->getHttp2MaxRequestBodySize();
}

int QoreSocketObject::setNonBlock(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    return priv->setNonBlock(xsink);
}

int QoreSocketObject::setNonBlock(ExceptionSink* xsink, unsigned direction) {
    AutoLocker al(priv->m);
    return priv->setNonBlock(xsink, direction);
}

void QoreSocketObject::clearNonBlock() {
    AutoLocker al(priv->m);
    priv->clearNonBlock();
}

void QoreSocketObject::clearNonBlock(unsigned direction) {
    AutoLocker al(priv->m);
    priv->clearNonBlock(direction);
}

bool QoreSocketObject::isQuic() const {
    AutoLocker al(priv->m);
    return priv->hasQuicSession();
}

void QoreSocketObject::shutdownAllQuicStreamReads() {
    // No priv->m here: shutdownStreamReads only touches per-session state
    // (streams_ map under each session's mtx_, plus the stream-data waiter
    // wake), and we deliberately don't want to serialize the broadcast
    // behind every other socket-method caller.  my_socket_priv::quic_sessions
    // is itself protected by its own quic_sessions_lock.
    priv->shutdownAllQuicStreamReads();
}

static int64_t qore_socket_object_submit_quic_request(QoreSocketObject* s, const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len, ExceptionSink* xsink,
        bool streaming, bool wake_controller, const char* owner_name) {
    // Fast path: see qore_socket_object_submit_http2_request for the full
    // rationale.  QuicSession::submitRequest* only enqueues protocol data; it
    // does not perform socket I/O, so it is safe in the async I/O execution
    // path and avoids nested synchronous Socket polling.
    bool on_io_thread = qore_on_async_io_thread();
    if (on_io_thread || qore_in_async_io_continue_poll_worker()) {
        std::shared_ptr<QuicSession> session = qore_socket_object_get_first_quic_session(s);
        if (!session) {
            xsink->raiseException("HTTP3-ERROR", "no HTTP/3 session active");
            return -1;
        }
        strcase_str_map_t header_map;
        qore_socket_object_set_quic_headers(header_map, headers);
        int64_t stream_id;
        if (streaming) {
            stream_id = session->submitRequestStreaming(method, path, header_map, xsink);
        } else {
            stream_id = session->submitRequest(method, path, header_map, body, body_len, xsink);
        }
        if (*xsink || stream_id < 0) {
            return stream_id;
        }
        if (wake_controller && !on_io_thread) {
            qore_socket_object_wake_async_controller(s, xsink);
            if (*xsink) {
                return -1;
            }
        }
        return stream_id;
    }

    return qore_socket_object_exec_quic_enqueue_int(s,
        new QoreSocketObjectQuicEnqueuePollOperation(s, method, path, headers, body, body_len, streaming,
            wake_controller),
        owner_name, xsink);
}

int64_t QoreSocketObject::submitQuicRequest(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len, ExceptionSink* xsink) {
    return qore_socket_object_submit_quic_request(this, method, path, headers, body, body_len, xsink, false, true,
        "submitQuicRequest");
}

int64_t QoreSocketObject::submitQuicRequestNoWake(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len, ExceptionSink* xsink) {
    return qore_socket_object_submit_quic_request(this, method, path, headers, body, body_len, xsink, false, false,
        "submitQuicRequest");
}

int64_t QoreSocketObject::submitQuicRequestStreaming(const char* method, const char* path,
        const QoreHashNode* headers, ExceptionSink* xsink) {
    return qore_socket_object_submit_quic_request(this, method, path, headers, nullptr, 0, xsink, true, true,
        "submitQuicRequestStreaming");
}

int64_t QoreSocketObject::submitQuicRequestStreamingNoWake(const char* method, const char* path,
        const QoreHashNode* headers, ExceptionSink* xsink) {
    return qore_socket_object_submit_quic_request(this, method, path, headers, nullptr, 0, xsink, true, false,
        "submitQuicRequestStreaming");
}

int QoreSocketObject::sendQuicClientStreamData(int64_t stream_id, const void* data,
        size_t len, bool end_stream, ExceptionSink* xsink) {
    return static_cast<int>(qore_socket_object_exec_quic_enqueue_int(this,
        new QoreSocketObjectQuicEnqueuePollOperation(this,
            QoreSocketObjectQuicEnqueuePollOperation::Action::ClientStreamData, 0, stream_id, data, len,
            end_stream),
        "sendQuicClientStreamData", xsink));
}

int QoreSocketObject::sendQuicClientStreamDataForAsyncPoll(int64_t stream_id, const void* data,
        size_t len, bool end_stream, ExceptionSink* xsink) {
    if (!qore_on_async_io_thread()) {
        return sendQuicClientStreamData(stream_id, data, len, end_stream, xsink);
    }

    std::shared_ptr<QuicSession> session = qore_socket_object_get_first_quic_session(this);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no active QUIC session on this socket");
        return -1;
    }
    return session->sendStreamData(stream_id, data, len, end_stream, xsink);
}

void QoreSocketObject::setQuicClientStreamStreamingDirect(int64_t stream_id) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_first_quic_session(this);
    if (session) {
        session->setStreamStreaming(stream_id);
    }
}

int QoreSocketObject::submitQuicClientTrailers(int64_t stream_id, const QoreHashNode* trailers,
        ExceptionSink* xsink) {
    return static_cast<int>(qore_socket_object_exec_quic_enqueue_int(this,
        new QoreSocketObjectQuicEnqueuePollOperation(this,
            QoreSocketObjectQuicEnqueuePollOperation::Action::Trailers, 0, stream_id, 0, trailers),
        "submitQuicClientTrailers", xsink));
}

bool QoreSocketObject::isQuicSessionClosed() const {
    ExceptionSink xsink;
    ValueHolder rv(qore_socket_object_exec_quic_query(const_cast<QoreSocketObject*>(this),
        new QoreSocketObjectQuicQueryPollOperation(const_cast<QoreSocketObject*>(this),
            QoreSocketObjectQuicQueryPollOperation::Action::FirstSessionClosed),
        "isQuicSessionClosed", &xsink), &xsink);
    if (xsink) {
        xsink.clear();
        return true;
    }
    return rv->getAsBool();
}

int QoreSocketObject::waitForQuicClientStreamDrain(int64_t stream_id, int timeout_ms,
        ExceptionSink* xsink) {
    SocketSyncPoll::assertNotOnIoThread("Socket", "waitForQuicClientStreamDrain", xsink);
    if (*xsink) {
        return -1;
    }

    std::string owner_name("waitForQuicClientStreamDrain:");
    owner_name += std::to_string(stream_id);
    return qore_socket_object_exec_stream_drain(this,
        new QoreSocketObjectStreamDrainPollOperation(this, stream_id), timeout_ms,
        owner_name.c_str(), xsink);
}

void QoreSocketObject::cancelQuicStream(int64_t session_id, int64_t stream_id, ExceptionSink* xsink) {
    qore_socket_object_exec_quic_enqueue_int(this,
        new QoreSocketObjectQuicEnqueuePollOperation(this,
            QoreSocketObjectQuicEnqueuePollOperation::Action::Cancel, session_id, stream_id),
        "cancelQuicStream", xsink);
}

int QoreSocketObject::cancelQuicStreamAsync(int64_t session_id, int64_t stream_id, ExceptionSink* xsink) {
    if (qore_on_async_io_thread()) {
        std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
        if (!session) {
            return 0;
        }
        return session->cancelStream(stream_id, NGHTTP3_H3_REQUEST_CANCELLED, xsink);
    }

    return static_cast<int>(qore_socket_object_exec_quic_enqueue_int(this,
        new QoreSocketObjectQuicEnqueuePollOperation(this,
            QoreSocketObjectQuicEnqueuePollOperation::Action::Cancel, session_id, stream_id),
        "cancelQuicStream", xsink));
}

void QoreSocketObject::cancelQuicStreamRead(int64_t session_id, int64_t stream_id, ExceptionSink* xsink) {
    // Resolve the session under the socket lock; missing session is a no-op
    // (idempotency for racing callers — e.g. service stop arriving after the
    // stream completed naturally).
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        return;
    }
    // shutdownStreamRead() is cross-thread safe: the per-stream flag write is
    // mutex-protected and the wake-up broadcast is delivered through the
    // standard async-I/O notifier path that any qore_on_async_io_thread()
    // poll-op observes.  No need to route through the async controller cmdq.
    session->shutdownStreamRead(stream_id);
    (void)xsink;
}

int QoreSocketObject::submitQuicResponse(int64_t session_id, int64_t stream_id, int status_code,
        const QoreHashNode* headers, const void* body, size_t body_len, ExceptionSink* xsink) {
    return static_cast<int>(qore_socket_object_exec_quic_enqueue_int(this,
        new QoreSocketObjectQuicEnqueuePollOperation(this,
            QoreSocketObjectQuicEnqueuePollOperation::Action::Response, session_id, stream_id, status_code,
            headers, body, body_len),
        "submitQuicResponse", xsink));
}

int64_t QoreSocketObject::getFirstQuicSessionId(ExceptionSink* xsink) const {
    ExceptionSink tmp_xsink;
    ExceptionSink* sink = xsink ? xsink : &tmp_xsink;
    ValueHolder rv(qore_socket_object_exec_quic_query(const_cast<QoreSocketObject*>(this),
        new QoreSocketObjectQuicQueryPollOperation(const_cast<QoreSocketObject*>(this),
            QoreSocketObjectQuicQueryPollOperation::Action::FirstSessionId),
        "getFirstQuicSessionId", sink), sink);
    if (*sink) {
        if (!xsink) {
            tmp_xsink.clear();
        }
        return 0;
    }
    return rv->getAsBigInt();
}

void QoreSocketObject::submitQuicShutdownNotice(int64_t session_id, ExceptionSink* xsink) {
    qore_socket_object_exec_quic_enqueue_int(this,
        new QoreSocketObjectQuicEnqueuePollOperation(this,
            QoreSocketObjectQuicEnqueuePollOperation::Action::ShutdownNotice, session_id),
        "submitQuicShutdownNotice", xsink);
}

void QoreSocketObject::submitQuicShutdown(int64_t session_id, ExceptionSink* xsink) {
    qore_socket_object_exec_quic_enqueue_int(this,
        new QoreSocketObjectQuicEnqueuePollOperation(this,
            QoreSocketObjectQuicEnqueuePollOperation::Action::Shutdown, session_id),
        "submitQuicShutdown", xsink);
}

QoreHashNode* QoreSocketObject::getQuicSessionGoawayState(int64_t session_id, ExceptionSink* xsink) {
    ValueHolder h(qore_socket_object_exec_quic_query(this,
        new QoreSocketObjectQuicQueryPollOperation(this,
            QoreSocketObjectQuicQueryPollOperation::Action::GoawayState, session_id),
        "getQuicSessionGoawayState", xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    return h->getType() == NT_HASH ? h.release().get<QoreHashNode>() : nullptr;
}

bool QoreSocketObject::isQuicGoawayReceived(int64_t session_id, ExceptionSink* xsink) {
    ValueHolder rv(qore_socket_object_exec_quic_query(this,
        new QoreSocketObjectQuicQueryPollOperation(this,
            QoreSocketObjectQuicQueryPollOperation::Action::GoawayReceived, session_id),
        "isQuicGoawayReceived", xsink), xsink);
    if (*xsink) {
        return false;
    }
    return rv->getAsBool();
}

QoreObject* QoreSocketObject::getQuicPeerCertificate(int64_t session_id, ExceptionSink* xsink) {
    ValueHolder cert(qore_socket_object_exec_quic_query(this,
        new QoreSocketObjectQuicQueryPollOperation(this,
            QoreSocketObjectQuicQueryPollOperation::Action::PeerCertificate, session_id),
        "getQuicPeerCertificate", xsink), xsink);
    if (*xsink || cert->isNothing()) {
        return nullptr;
    }
    return cert->getType() == NT_OBJECT ? cert.release().get<QoreObject>() : nullptr;
}

int QoreSocketObject::submitQuicResponseStreaming(int64_t session_id, int64_t stream_id,
        int status_code, const QoreHashNode* headers, ExceptionSink* xsink) {
    return static_cast<int>(qore_socket_object_exec_quic_enqueue_int(this,
        new QoreSocketObjectQuicEnqueuePollOperation(this,
            QoreSocketObjectQuicEnqueuePollOperation::Action::ResponseStreaming, session_id, stream_id,
            status_code, headers),
        "submitQuicResponseStreaming", xsink));
}

int QoreSocketObject::submitQuicStreamData(int64_t session_id, int64_t stream_id,
        const void* data, size_t len, bool end_stream, ExceptionSink* xsink) {
    return static_cast<int>(qore_socket_object_exec_quic_enqueue_int(this,
        new QoreSocketObjectQuicEnqueuePollOperation(this,
            QoreSocketObjectQuicEnqueuePollOperation::Action::StreamData, session_id, stream_id, data, len,
            end_stream),
        "submitQuicStreamData", xsink));
}

void QoreSocketObject::setQuicStreamInputStream(int64_t session_id, int64_t stream_id,
        InputStream* body, ExceptionSink* xsink) {
    SimpleRefHolder<InputStream> body_holder(body);

    if (!body->isIoThreadSafe()) {
        xsink->raiseException("QUIC-ERROR", "InputStream is not I/O thread safe");
        return;
    }

    body->unassignThread(xsink);
    if (*xsink) {
        return;
    }

    qore_socket_object_exec_quic_enqueue_int(this,
        new QoreSocketObjectQuicEnqueuePollOperation(this, session_id, stream_id, body_holder.release(), false),
        "setQuicStreamInputStream", xsink);
}

int QoreSocketObject::submitQuicStreamingResponseWithStream(int64_t session_id, int64_t stream_id,
        int status_code, const QoreHashNode* headers, InputStream* body, ExceptionSink* xsink) {
    if (!body->isIoThreadSafe()) {
        return 1;
    }

    body->unassignThread(xsink);
    if (*xsink) {
        return -1;
    }

    return static_cast<int>(qore_socket_object_exec_quic_enqueue_int(this,
        new QoreSocketObjectQuicEnqueuePollOperation(this, session_id, stream_id, status_code, headers, body,
            true),
        "submitQuicStreamingResponseWithStream", xsink));
}

int QoreSocketObject::waitForQuicStreamDrain(int64_t session_id, int64_t stream_id,
        int timeout_ms, ExceptionSink* xsink) {
    SocketSyncPoll::assertNotOnIoThread("Socket", "waitForQuicStreamDrain", xsink);
    if (*xsink) {
        return -1;
    }
    std::string owner_name("waitForQuicStreamDrain:");
    owner_name += std::to_string(session_id);
    owner_name += ':';
    owner_name += std::to_string(stream_id);
    return qore_socket_object_exec_stream_drain(this,
        new QoreSocketObjectStreamDrainPollOperation(this, session_id, stream_id), timeout_ms,
        owner_name.c_str(), xsink);
}

int QoreSocketObject::submitQuicConnectResponse(int64_t session_id, int64_t stream_id,
        int status_code, const QoreHashNode* headers, ExceptionSink* xsink) {
    return static_cast<int>(qore_socket_object_exec_quic_enqueue_int(this,
        new QoreSocketObjectQuicEnqueuePollOperation(this,
            QoreSocketObjectQuicEnqueuePollOperation::Action::ConnectResponse, session_id, stream_id,
            status_code, headers),
        "submitQuicConnectResponse", xsink));
}

QoreValue QoreSocketObject::readQuicConnectStreamData(int64_t session_id, int64_t stream_id,
        ExceptionSink* xsink) {
    return qore_socket_object_exec_quic_connect_stream_data(this,
        new QoreSocketObjectQuicConnectStreamDataPollOperation(this, session_id, stream_id),
        session_id, stream_id, xsink);
}

void QoreSocketObject::registerQuicConnectStreamQueue(int64_t session_id, int64_t stream_id,
        Queue* queue, ExceptionSink* xsink) {
    qore_socket_object_exec_quic_enqueue_int(this,
        new QoreSocketObjectQuicEnqueuePollOperation(this,
            QoreSocketObjectQuicEnqueuePollOperation::Action::RegisterConnectQueue, session_id, stream_id, queue),
        "registerQuicConnectStreamQueue", xsink);
}

void QoreSocketObject::deregisterQuicConnectStreamQueue(int64_t session_id, int64_t stream_id,
        ExceptionSink* xsink) {
    qore_socket_object_exec_quic_enqueue_int(this,
        new QoreSocketObjectQuicEnqueuePollOperation(this,
            QoreSocketObjectQuicEnqueuePollOperation::Action::DeregisterConnectQueue, session_id, stream_id),
        "deregisterQuicConnectStreamQueue", xsink);
}

void QoreSocketObject::registerQuicConnectStreamFrameState(int64_t session_id, int64_t stream_id,
        Queue* msg_queue, ExceptionSink* xsink) {
    qore_socket_object_exec_quic_enqueue_int(this,
        new QoreSocketObjectQuicEnqueuePollOperation(this,
            QoreSocketObjectQuicEnqueuePollOperation::Action::RegisterConnectFrameState, session_id, stream_id,
            msg_queue),
        "registerQuicConnectStreamFrameState", xsink);
}

void QoreSocketObject::deregisterQuicConnectStreamFrameState(int64_t session_id, int64_t stream_id,
        ExceptionSink* xsink) {
    qore_socket_object_exec_quic_enqueue_int(this,
        new QoreSocketObjectQuicEnqueuePollOperation(this,
            QoreSocketObjectQuicEnqueuePollOperation::Action::DeregisterConnectFrameState, session_id, stream_id),
        "deregisterQuicConnectStreamFrameState", xsink);
}

BinaryNode* QoreSocketObject::readQuicStreamDataBlock(int64_t session_id, int64_t stream_id,
        int timeout_ms, ExceptionSink* xsink) {
    return qore_socket_object_exec_quic_stream_data(this,
        new QoreSocketObjectQuicStreamDataPollOperation(this, session_id, stream_id), timeout_ms,
        session_id, stream_id, xsink);
}

bool QoreSocketObject::isQuicStreamComplete(int64_t session_id, int64_t stream_id) const {
    ExceptionSink xsink;
    ValueHolder rv(qore_socket_object_exec_quic_query(const_cast<QoreSocketObject*>(this),
        new QoreSocketObjectQuicQueryPollOperation(const_cast<QoreSocketObject*>(this),
            QoreSocketObjectQuicQueryPollOperation::Action::StreamComplete, session_id, stream_id),
        "isQuicStreamComplete", &xsink), &xsink);
    if (xsink) {
        xsink.clear();
        return true;
    }
    return rv->getAsBool();
}

void QoreSocketObject::cleanupQuicStream(int64_t session_id, int64_t stream_id,
        ExceptionSink* xsink) {
    qore_socket_object_exec_quic_enqueue_int(this,
        new QoreSocketObjectQuicEnqueuePollOperation(this,
            QoreSocketObjectQuicEnqueuePollOperation::Action::Cleanup, session_id, stream_id),
        "cleanupQuicStream", xsink);
}

int QoreSocketObject::resetQuicStream(int64_t session_id, int64_t stream_id,
        ExceptionSink* xsink) {
    return static_cast<int>(qore_socket_object_exec_quic_enqueue_int(this,
        new QoreSocketObjectQuicEnqueuePollOperation(this,
            QoreSocketObjectQuicEnqueuePollOperation::Action::Reset, session_id, stream_id),
        "resetQuicStream", xsink));
}

int QoreSocketObject::submitQuicDatagram(int64_t session_id, int64_t stream_id,
        const BinaryNode* data, ExceptionSink* xsink) {
    return static_cast<int>(qore_socket_object_exec_quic_enqueue_int(this,
        new QoreSocketObjectQuicEnqueuePollOperation(this,
            QoreSocketObjectQuicEnqueuePollOperation::Action::Datagram, session_id, stream_id,
            data ? data->getPtr() : nullptr, data ? data->size() : 0, false),
        "submitQuicDatagram", xsink));
}

QoreValue QoreSocketObject::readQuicDatagram(int64_t session_id, int64_t stream_id,
        int timeout_ms, ExceptionSink* xsink) {
    return qore_socket_object_exec_quic_datagram(this,
        new QoreSocketObjectQuicDatagramPollOperation(this, session_id, stream_id), timeout_ms, session_id, stream_id,
        xsink);
}

void QoreSocketObject::registerQuicDatagramQueue(int64_t session_id, int64_t stream_id,
        Queue* queue, ExceptionSink* xsink) {
    qore_socket_object_exec_quic_enqueue_int(this,
        new QoreSocketObjectQuicEnqueuePollOperation(this,
            QoreSocketObjectQuicEnqueuePollOperation::Action::RegisterDatagramQueue, session_id, stream_id, queue),
        "registerQuicDatagramQueue", xsink);
}

void QoreSocketObject::unregisterQuicDatagramQueue(int64_t session_id, int64_t stream_id,
        ExceptionSink* xsink) {
    qore_socket_object_exec_quic_enqueue_int(this,
        new QoreSocketObjectQuicEnqueuePollOperation(this,
            QoreSocketObjectQuicEnqueuePollOperation::Action::UnregisterDatagramQueue, session_id, stream_id),
        "unregisterQuicDatagramQueue", xsink);
}

int64_t QoreSocketObject::getQuicMaxDatagramSize(int64_t session_id, int64_t stream_id,
        ExceptionSink* xsink) {
    ValueHolder rv(qore_socket_object_exec_quic_query(this,
        new QoreSocketObjectQuicQueryPollOperation(this,
            QoreSocketObjectQuicQueryPollOperation::Action::MaxDatagramSize, session_id, stream_id),
        "getQuicMaxDatagramSize", xsink), xsink);
    if (*xsink) {
        return 0;
    }
    return rv->getAsBigInt();
}

bool QoreSocketObject::isQuicDatagramSupported(int64_t session_id, ExceptionSink* xsink) {
    ValueHolder rv(qore_socket_object_exec_quic_query(this,
        new QoreSocketObjectQuicQueryPollOperation(this,
            QoreSocketObjectQuicQueryPollOperation::Action::DatagramSupported, session_id),
        "isQuicDatagramSupported", xsink), xsink);
    if (*xsink) {
        return false;
    }
    return rv->getAsBool();
}

bool my_socket_priv::hasQuicSession() const {
    qore_socket_private* sp = qore_socket_private::get(*socket);
    AutoLocker al(sp->quic_sessions_lock);
    return !sp->quic_sessions.empty();
}

void my_socket_priv::shutdownAllQuicStreamReads() {
    // Snapshot the live sessions under the map lock, then call
    // shutdownStreamReads() on each without holding the map lock — the call
    // takes the per-session mtx_ to set per-stream flags and broadcasts the
    // stream-data wake, and we don't want to block the map for that.
    std::vector<std::shared_ptr<QuicSession>> sessions;
    qore_socket_private* sp = qore_socket_private::get(*socket);
    {
        AutoLocker al(sp->quic_sessions_lock);
        sessions.reserve(sp->quic_sessions.size());
        for (auto& [id, sess] : sp->quic_sessions) {
            (void)id;
            sessions.push_back(sess);
        }
    }
    for (auto& sess : sessions) {
        sess->shutdownStreamReads();
    }
}

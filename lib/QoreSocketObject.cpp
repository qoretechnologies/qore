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

#include <limits>

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

class QoreSocketObjectStreamDrainPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketObjectStreamDrainPollOperation(Http2SessionPtr h2, int64_t stream_id)
            : h2(std::move(h2)), stream_id(stream_id) {
    }

    DLLLOCAL QoreSocketObjectStreamDrainPollOperation(std::shared_ptr<QuicSession> quic, int64_t stream_id)
            : quic(std::move(quic)), stream_id(stream_id) {
    }

    DLLLOCAL ~QoreSocketObjectStreamDrainPollOperation() override {
        ExceptionSink xsink;
        cleanup(&xsink);
        if (xsink) {
            xsink.clear();
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

        int rc = checkDrain();
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

        rc = checkDrain();
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
    DLLLOCAL int checkDrain() const {
        return h2
            ? h2->waitForStreamDrain(static_cast<int32_t>(stream_id), 0)
            : quic->waitForStreamDrain(stream_id, 0);
    }

    DLLLOCAL int registerWaiter(ExceptionSink* xsink) {
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
            } else {
                quic->unregisterStreamDrainWaiter(waiter_sock_obj, xsink);
            }
            registered = false;
        }
        if (waiter_sock_obj) {
            waiter_sock_obj->deref(xsink);
            waiter_sock_obj = nullptr;
        }
    }

    Http2SessionPtr h2;
    std::shared_ptr<QuicSession> quic;
    QoreObject* waiter_sock_obj = nullptr;
    int64_t stream_id;
    int output = -1;
    bool done = false;
    bool registered = false;
    bool wake_requested = false;
};

class QoreSocketObjectQuicStreamDataPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketObjectQuicStreamDataPollOperation(std::shared_ptr<QuicSession> session,
            int64_t session_id, int64_t stream_id)
            : session(std::move(session)), session_id(session_id), stream_id(stream_id) {
    }

    DLLLOCAL ~QoreSocketObjectQuicStreamDataPollOperation() override {
        ExceptionSink xsink;
        cleanup(&xsink);
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
    DLLLOCAL int checkData(ExceptionSink* xsink) {
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
        if (registered && waiter_sock_obj) {
            session->unregisterStreamDataWaiter(waiter_sock_obj, xsink);
            registered = false;
        }
        if (waiter_sock_obj) {
            waiter_sock_obj->deref(xsink);
            waiter_sock_obj = nullptr;
        }
    }

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
    DLLLOCAL QoreSocketObjectQuicDatagramPollOperation(std::shared_ptr<QuicSession> session,
            int64_t stream_id)
            : session(std::move(session)), stream_id(stream_id) {
    }

    DLLLOCAL ~QoreSocketObjectQuicDatagramPollOperation() override {
        ExceptionSink xsink;
        cleanup(&xsink);
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
    DLLLOCAL int checkData(ExceptionSink* xsink) {
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
        if (registered && waiter_sock_obj) {
            session->unregisterDatagramWaiter(waiter_sock_obj, xsink);
            registered = false;
        }
        if (waiter_sock_obj) {
            waiter_sock_obj->deref(xsink);
            waiter_sock_obj = nullptr;
        }
    }

    std::shared_ptr<QuicSession> session;
    SimpleRefHolder<BinaryNode> data;
    QoreObject* waiter_sock_obj = nullptr;
    int64_t stream_id;
    bool done = false;
    bool registered = false;
    bool wake_requested = false;
};

static QoreObject* qore_socket_object_make_pollable_wrapper(QoreSocketObject* s) {
    s->ref();
    return new QoreObject(QC_ABSTRACTPOLLABLEIOOBJECTBASE, getProgram(), s);
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

static QoreHashNode* qore_socket_object_make_sockaddr_output(const struct sockaddr_storage& addr, socklen_t len,
        const std::string& socketname) {
    QoreHashNode* h = new QoreHashNode(autoTypeInfo);
    BinaryNode* b = new BinaryNode();
    b->append(&addr, sizeof(addr));
    h->setKeyValue("addr", b, nullptr);
    h->setKeyValue("len", static_cast<int64>(len), nullptr);
    if (!socketname.empty()) {
        h->setKeyValue("socketname", new QoreStringNode(socketname), nullptr);
    }
    return h;
}

class QoreSocketObjectAddressInfoPollOperation : public SocketPollOperationBase {
public:
    enum class Action {
        Peer,
        Socket,
    };

    DLLLOCAL QoreSocketObjectAddressInfoPollOperation(QoreSocketObject* sock, Action action)
            : sock(sock), action(action) {
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
        if (!rc) {
            socketname = sp->socketname;
            success = true;
        }
        done = true;
        return nullptr;
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return success ? qore_socket_object_make_sockaddr_output(addr, len, socketname) : QoreValue();
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        if (done) {
            return "done";
        }
        return action == Action::Peer ? "getting-peer-info" : "getting-socket-info";
    }

private:
    QoreSocketObject* sock;
    Action action;
    struct sockaddr_storage addr = {};
    socklen_t len = 0;
    std::string socketname;
    bool done = false;
    bool success = false;
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
    ReferenceHolder<QoreHashNode> result(qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj,
        timeout_ms, "readQuicStreamDataBlock", xsink, &ex), xsink);
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
        QoreSocketObjectQuicDatagramPollOperation* datagram_poller, int timeout_ms, ExceptionSink* xsink) {
    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, datagram_poller, "quic-datagram", xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }

    QoreHashNode* ex = nullptr;
    ReferenceHolder<QoreHashNode> result(qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj,
        timeout_ms, "readQuicDatagram", xsink, &ex), xsink);
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

static int qore_socket_object_exec_connect(QoreSocketObject* s, const char* target, int timeout_ms, bool ssl,
        ExceptionSink* xsink) {
    s->ref();
    const char* goal = ssl ? "connect-ssl" : "connect";
    return qore_socket_object_exec_poll_no_output(s, new SocketConnectPollOperation(xsink, ssl, target, s, true),
        timeout_ms, goal, goal, xsink);
}

static int qore_socket_object_exec_connect_inet(QoreSocketObject* s, const char* host, const char* service,
        int family, int socktype, int protocol, int timeout_ms, bool ssl, ExceptionSink* xsink) {
    s->ref();
    const char* goal = ssl ? "connect-ssl" : "connect";
    return qore_socket_object_exec_poll_no_output(s,
        new SocketConnectPollOperation(xsink, ssl, host, service, family, socktype, protocol, s, true),
        timeout_ms, goal, goal, xsink);
}

static int qore_socket_object_exec_connect_unix(QoreSocketObject* s, const char* path, int socktype, int protocol,
        int timeout_ms, bool ssl, ExceptionSink* xsink) {
    s->ref();
    const char* goal = ssl ? "connect-ssl" : "connect";
    return qore_socket_object_exec_poll_no_output(s,
        new SocketConnectPollOperation(xsink, ssl, path, socktype, protocol, s, true), timeout_ms, goal, goal,
        xsink);
}

static int qore_socket_object_exec_upgrade_ssl(QoreSocketObject* s, int timeout_ms, bool server,
        ExceptionSink* xsink) {
    s->ref();
    const char* goal = server ? "upgrade-server-ssl" : "upgrade-client-ssl";
    return qore_socket_object_exec_poll_no_output(s,
        server
            ? static_cast<SocketPollOperationBase*>(new SocketUpgradeServerSslPollOperation(xsink, s, true))
            : static_cast<SocketPollOperationBase*>(new SocketUpgradeClientSslPollOperation(xsink, s, true)),
        timeout_ms, goal, goal, xsink);
}

static int qore_socket_object_exec_shutdown_ssl(QoreSocketObject* s, ExceptionSink* xsink) {
    s->ref();
    return qore_socket_object_exec_poll_no_output(s, new SocketShutdownSslPollOperation(xsink, s, true), -1,
        "shutdownSSL", "shutdown-ssl", xsink);
}

static int qore_socket_object_exec_http2_flush(QoreSocketObject* s, const char* owner_name, ExceptionSink* xsink,
        bool submit_ping = false) {
    s->ref();
    return qore_socket_object_exec_poll_no_output(s, new SocketHttp2FlushPollOperation(xsink, s, true, submit_ping),
        -1, owner_name, "done", xsink);
}

static int qore_socket_object_exec_shutdown(QoreSocketObject* s, ExceptionSink* xsink) {
    s->ref();
    SocketShutdownPollOperation* shutdown_poller = new SocketShutdownPollOperation(s);
    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, shutdown_poller, "shutdown", xsink), xsink);
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
    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, setup_poller, goal, xsink), xsink);
    if (*xsink) {
        return -1;
    }

    ReferenceHolder<QoreHashNode> result(
        qore_socket_object_exec_poll_operation(s, *sock_obj, *op_obj, -1, owner_name, xsink), xsink);
    return *xsink ? -1 : setup_poller->getRc();
}

static QoreHashNode* qore_socket_object_get_addr_info_from_output(const QoreValue output, bool host_lookup,
        const char* err, ExceptionSink* xsink) {
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

    return qore_socket_private::getAddrInfo(addr, static_cast<socklen_t>(raw_len), host_lookup, socketname);
}

static QoreHashNode* qore_socket_object_exec_address_info(QoreSocketObject* s,
        QoreSocketObjectAddressInfoPollOperation::Action action, bool host_lookup, const char* owner_name,
        const char* err, ExceptionSink* xsink) {
    if (qore_on_async_io_thread()) {
        my_socket_priv* priv = my_socket_priv::getPriv(*s);
        AutoLocker al(priv->m);
        return action == QoreSocketObjectAddressInfoPollOperation::Action::Peer
            ? priv->socket->getPeerInfo(xsink, host_lookup)
            : priv->socket->getSocketInfo(xsink, host_lookup);
    }

    s->ref();
    QoreSocketObjectAddressInfoPollOperation* poller = new QoreSocketObjectAddressInfoPollOperation(s, action);
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
    return *xsink ? nullptr : qore_socket_object_get_addr_info_from_output(*output, host_lookup, err, xsink);
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

    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    const char* goal = ssl ? "accept-ssl" : "accept";
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, accept_poller, goal, xsink), xsink);
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
        int timeout_ms, ExceptionSink* xsink) {
    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, poller, "send", xsink), xsink);
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
    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, poller, "received", xsink), xsink);
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

class QoreSocketObjectAsyncIoGuard {
public:
    QoreSocketObjectAsyncIoGuard(my_socket_priv& priv, ExceptionSink* xsink, unsigned direction)
            : priv(priv), direction(direction) {
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

static QoreHashNode* qore_socket_object_exec_read_http_header(QoreSocketObject* s, QoreHashNode* info,
        int timeout_ms, ExceptionSink* xsink) {
    s->ref();
    SocketReadHttpHeaderPollOperation* header_poller = new SocketReadHttpHeaderPollOperation(xsink, s, true);

    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, header_poller, "received", xsink), xsink);
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

static bool qore_socket_object_exec_process_sse_char(my_socket_priv* priv, QoreString& str, int& eol_count,
        char c) {
    if (priv->takeSseGotCr()) {
        if (c == '\n') {
            return false;
        }
    }

    if (c == '\r') {
        str.concat('\n');
        priv->setSseGotCr(true);
        return ++eol_count == 2;
    }

    if (c == '\n') {
        str.concat('\n');
        return ++eol_count == 2;
    }

    if (eol_count) {
        eol_count = 0;
    }
    str.concat(c);
    return false;
}

static QoreHashNode* qore_socket_object_exec_read_server_sent_event(QoreSocketObject* s, int timeout_ms,
        ExceptionSink* xsink) {
    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_RECV);
    if (!async_guard) {
        return nullptr;
    }

    QoreString str(QCS_UTF8);
    int eol_count = 0;
    unsigned cancel_check = 0;
    while (true) {
        if (!(cancel_check++ % 100) && qore_check_cancel(xsink, "socket SSE read")) {
            return nullptr;
        }

        SimpleRefHolder<BinaryNode> data(qore_socket_object_exec_recv_bytes(s, 1, timeout_ms, xsink));
        if (*xsink) {
            return nullptr;
        }
        char c = *static_cast<const char*>(data->getPtr());
        if (qore_socket_object_exec_process_sse_char(priv, str, eol_count, c)) {
            break;
        }
    }

    return QoreSocket::parseServerSentEvent(xsink, str);
}

static QoreHashNode* qore_socket_object_exec_read_server_sent_event_encoded(QoreSocketObject* s,
        const QoreStringNode* content_encoding, int timeout_ms, ExceptionSink* xsink) {
    SimpleRefHolder<Transform> transform(CompressionTransforms::getDecompressor(content_encoding, xsink));
    if (*xsink) {
        return nullptr;
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*s);
    QoreSocketObjectAsyncIoGuard async_guard(*priv, xsink, NB_RECV);
    if (!async_guard) {
        return nullptr;
    }

    QoreString str(QCS_UTF8);
    int eol_count = 0;

    size_t tbufsize = transform->outputBufferSize();
    char* tbuf = tbufsize ? static_cast<char*>(malloc(tbufsize * sizeof(char))) : nullptr;
    if (tbufsize && !tbuf) {
        xsink->outOfMemory();
        return nullptr;
    }
    ON_BLOCK_EXIT(free, tbuf);
    size_t tlen = 0;
    size_t tpos = 0;
    QoreString cibuf;

    unsigned cancel_check = 0;
    while (true) {
        if (!(cancel_check++ % 100) && qore_check_cancel(xsink, "socket encoded SSE read")) {
            return nullptr;
        }

        if (tpos >= tlen) {
            SimpleRefHolder<BinaryNode> data(qore_socket_object_exec_recv_some_binary(s, DEFAULT_SOCKET_BUFSIZE,
                timeout_ms, xsink, "readServerSentEvent"));
            if (*xsink) {
                return nullptr;
            }
            if (!data->size()) {
                se_closed("Socket", "readServerSentEvent", xsink);
                return nullptr;
            }

            cibuf.concat(static_cast<const char*>(data->getPtr()), data->size());
            tpos = 0;
            std::pair<int64, int64> result = transform->apply(cibuf.c_str(), cibuf.size(), tbuf, tbufsize, xsink);
            if (*xsink) {
                return nullptr;
            }
            if (result.first) {
                cibuf.removeBytes(result.first);
            }
            if (!result.second) {
                continue;
            }
            tlen = static_cast<size_t>(result.second);
        }

        char c = tbuf[tpos++];
        if (qore_socket_object_exec_process_sse_char(priv, str, eol_count, c)) {
            break;
        }
    }

    return QoreSocket::parseServerSentEvent(xsink, str);
}

static int qore_socket_object_exec_send_http_message(QoreSocketObject* s, QoreHashNode* info,
        const char* method, const char* path, const char* http_version, const QoreHashNode* headers,
        const void* data, size_t size, const QoreStringNode* body_event, int source, int timeout_ms,
        ExceptionSink* xsink) {
    QoreString hdr(s->getEncoding());
    if (my_socket_priv::getPriv(*s)->getSendHttpMessageHeaders(xsink, hdr, info, method, path, http_version,
            headers, size, source)) {
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
    int32_t stream_id = priv->getH2ActiveServerStreamId();
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
            new SocketHttp2SendResponsePollOperation(xsink, s, nullptr, stream_id, code, headers, *body_bin),
            timeout_ms, xsink);
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
    QoreString hdr(s->getEncoding());
    hdr.concat("0\r\n");
    qore_socket_private::do_headers(hdr, trailer, 0, false);

    int rc = qore_socket_object_exec_send_bytes(s, hdr.c_str(), hdr.size(), timeout_ms, xsink);
    if (!rc && trailer) {
        my_socket_priv::getPriv(*s)->doHeaderEvent(QORE_EVENT_HTTP_FOOTERS_SENT, source, *trailer);
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

    QoreString hdr(s->getEncoding());
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

    if (priv->getH2ActiveServerStreamId() > 0) {
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

    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, readiness_poller, owner_name, xsink), xsink);
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

    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, data_available_poller, "isDataAvailable", xsink), xsink);
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

    ReferenceHolder<QoreObject> sock_obj(qore_socket_object_make_pollable_wrapper(s), xsink);
    ReferenceHolder<QoreObject> op_obj(
        qore_socket_object_make_poll_op(*sock_obj, idle_poller, "check-idle-data", xsink), xsink);
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
    if (priv->socket->priv->buflen > priv->socket->priv->bufoffset) {
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
    if (sp->buflen <= sp->bufoffset) {
        return nullptr;
    }
    size_t avail = sp->buflen - sp->bufoffset;
    BinaryNode* result = new BinaryNode(sp->rbuf + sp->bufoffset, avail);
    sp->bufoffset = 0;
    sp->buflen = 0;
    return result;
}

void QoreSocketObject::invalidate(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    priv->invalidate();
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
    if (qore_on_async_io_thread()) {
        AutoLocker al(priv->m);
        return priv->socket->getPort();
    }

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
    if (qore_on_async_io_thread()) {
        AutoLocker al(priv->m);
        return priv->socket->setSendTimeout(ms);
    }

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
    if (qore_on_async_io_thread()) {
        AutoLocker al(priv->m);
        return priv->socket->setRecvTimeout(ms);
    }

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
    if (qore_on_async_io_thread()) {
        AutoLocker al(priv->m);
        return priv->socket->getSendTimeout();
    }

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
    if (qore_on_async_io_thread()) {
        AutoLocker al(priv->m);
        return priv->socket->getRecvTimeout();
    }

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
    AutoLocker al(priv->m);
    return priv->socket->getSSLCipherName();
}

const char* QoreSocketObject::getSSLCipherVersion() {
    AutoLocker al(priv->m);
    return priv->socket->getSSLCipherVersion();
}

bool QoreSocketObject::isSecure() {
    AutoLocker al(priv->m);
    return priv->socket->isSecure();
}

int QoreSocketObject::checkIdleData(ExceptionSink* xsink) {
    return qore_socket_object_exec_check_idle_data(this, xsink);
}

int QoreSocketObject::checkIdleDataForAsyncPoll(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
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
    return 0;
}

void QoreSocketObject::setAlpnProtocols(const QoreListNode* protocols, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    priv->socket->setAlpnProtocols(protocols, xsink);
}

QoreStringNode* QoreSocketObject::getAlpnProtocol() const {
    AutoLocker al(priv->m);
    return priv->socket->getAlpnProtocol();
}

bool QoreSocketObject::isHttp2() const {
    AutoLocker al(priv->m);
    return priv->socket->isHttp2();
}

int32_t QoreSocketObject::submitHttp2PushPromise(int32_t stream_id, const char* path,
        const QoreHashNode* headers, ExceptionSink* xsink) {
    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    if (!h2) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
        return -1;
    }

    strcase_str_map_t h2_headers;
    qore_socket_object_set_h2_headers(h2_headers, headers);
    return h2->submitPushPromise(stream_id, path, h2_headers, xsink);
}

int QoreSocketObject::submitHttp2Response(int32_t stream_id, int status_code,
        const QoreHashNode* headers, const void* body, size_t body_len,
        ExceptionSink* xsink) {
    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    if (!h2) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
        return -1;
    }

    strcase_str_map_t h2_headers;
    qore_socket_object_set_h2_headers(h2_headers, headers);
    return h2->submitResponse(stream_id, status_code, h2_headers, body, body_len, xsink);
}

int QoreSocketObject::submitHttp2ConnectResponse(int32_t stream_id, int status_code,
        const QoreHashNode* headers, ExceptionSink* xsink) {
    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    if (!h2) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
        return -1;
    }

    strcase_str_map_t h2_headers;
    qore_socket_object_set_h2_headers(h2_headers, headers);
    return h2->submitConnectResponse(stream_id, status_code, h2_headers, xsink);
}

int32_t QoreSocketObject::submitHttp2Request(const QoreHashNode* headers, const void* body,
        size_t body_len, ExceptionSink* xsink, bool streaming) {
    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    if (!h2) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
        return -1;
    }

    if (!headers) {
        xsink->raiseException("HTTP2-ERROR", "HTTP/2 request headers are required");
        return -1;
    }

    std::vector<std::pair<std::string, std::string>> h2_headers;
    h2_headers.reserve(headers->size() + 4);
    std::string method;
    std::string path;

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
        h2_headers.emplace_back(key, sval ? sval : "");
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

    return h2->submitRequest(method.c_str(), path.c_str(), h2_headers, body, body_len, xsink, streaming);
}

void QoreSocketObject::cancelHttp2Stream(int32_t stream_id, ExceptionSink* xsink) {
    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    if (!h2) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
        return;
    }
    h2->submitRstStream(stream_id, NGHTTP2_CANCEL, xsink);
}

void QoreSocketObject::setHttp2StreamStreaming(int32_t stream_id) {
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
    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    if (!h2) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
        return nullptr;
    }
    return h2->takeStreamData(stream_id, max_bytes, xsink);
}

int QoreSocketObject::sendHttp2Trailers(int32_t stream_id, const QoreHashNode* trailers,
        ExceptionSink* xsink) {
    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    if (!h2) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
        return -1;
    }

    strcase_str_map_t trailer_map;
    qore_socket_object_set_h2_headers(trailer_map, trailers);
    return h2->submitTrailers(stream_id, trailer_map, xsink);
}

int QoreSocketObject::flushHttp2PendingData(ExceptionSink* xsink) {
    if (!qore_on_async_io_thread()) {
        Http2SessionPtr h2;
        {
            AutoLocker al(priv->m);
            qore_socket_private* sp = qore_socket_private::get(*priv->socket);
            h2 = sp->h2_session;
        }
        if (!h2) {
            return 0;
        }
        return qore_socket_object_exec_http2_flush(this, "flushHttp2PendingData", xsink);
    }

    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    if (!h2) {
        return 0;
    }
    return h2->sendPendingData(0, xsink);
}

int QoreSocketObject::submitHttp2Ping(ExceptionSink* xsink) {
    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    if (!h2) {
        return 0;
    }

    if (!qore_on_async_io_thread()) {
        return qore_socket_object_exec_http2_flush(this, "submitHttp2Ping", xsink, true);
    }
    int rv = h2->submitPing(nullptr, xsink);
    if (rv < 0 || *xsink) {
        return -1;
    }
    return h2->sendPendingData(0, xsink);
}

// Async-path H2 server write methods.  These follow the waitForHttp2StreamDrain
// precedent (see below): acquire priv->m only briefly to copy the
// Http2Session shared pointer, then perform header/data/trailer submission
// under only the session's internal recursive mutex.  This eliminates the
// handler-thread vs. I/O-thread contention on priv->m that motivated the
// GrpcServer async-migration (see design/grpc-server-async-migration.md).
int QoreSocketObject::submitHttp2StreamingResponseHeadersAsync(int32_t stream_id, int status_code,
        const QoreHashNode* headers, ExceptionSink* xsink) {
    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    if (!h2) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
        return -1;
    }
    strcase_str_map_t header_map;
    qore_socket_object_set_h2_headers(header_map, headers);
    return h2->submitResponseStreaming(stream_id, status_code, header_map, xsink);
}

int QoreSocketObject::sendHttp2StreamDataAsync(int32_t stream_id, const BinaryNode* data,
        bool end_stream, ExceptionSink* xsink) {
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
        xsink->raiseException("HTTP2-FLOW-CONTROL",
            "stream %d buffer full: data dropped", stream_id);
        return -1;
    }
    return 0;
}

int QoreSocketObject::sendHttp2TrailersAsync(int32_t stream_id, const QoreHashNode* trailers,
        ExceptionSink* xsink) {
    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    if (!h2) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
        return -1;
    }
    strcase_str_map_t trailer_map;
    qore_socket_object_set_h2_headers(trailer_map, trailers);
    return h2->submitTrailers(stream_id, trailer_map, xsink);
}

void QoreSocketObject::cleanupHttp2StreamAsync(int32_t stream_id) {
    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    if (h2) {
        h2->cleanupStream(stream_id);
    }
}

int QoreSocketObject::resetHttp2StreamAsync(int32_t stream_id, ExceptionSink* xsink) {
    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    if (!h2) {
        return 0;
    }
    int rv = h2->submitRstStream(stream_id, NGHTTP2_CANCEL, xsink);
    if (rv != 0) {
        printd(2, "resetHttp2StreamAsync() submitRstStream failed for stream %d (rv=%d), "
            "cleaning up local state anyway\n", stream_id, rv);
    }
    h2->cleanupStream(stream_id);
    return rv;
}

int QoreSocketObject::submitHttp2StreamingResponseWithStream(int32_t stream_id, int status_code,
        const QoreHashNode* headers, InputStream* body, ExceptionSink* xsink) {
    // C++ vtable is the sole authority on I/O thread eligibility.
    // Return 1 (not accepted) so the caller can fall back to handler-thread streaming.
    // No exception — the caller handles the fallback path.
    if (!body->isIoThreadSafe()) {
        return 1;
    }

    // Follow the same brief-lock pattern as the other async-safe HTTP/2
    // server write methods: acquire priv->m only long enough to copy the
    // Http2Session shared pointer, then do all session work under only
    // Http2Session's own recursive mutex.  The handler thread never
    // competes with the I/O thread for priv->m during frame submission
    // or InputStream registration.
    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    if (!h2) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session available");
        return -1;
    }

    // Submit response headers without END_STREAM
    strcase_str_map_t header_map;
    qore_socket_object_set_h2_headers(header_map, headers);
    int rv = h2->submitResponseStreaming(stream_id, status_code, header_map, xsink);
    if (rv) {
        return rv;
    }

    // Extract Content-Length from headers if declared, so the session can
    // enforce it and send RST_STREAM on short-stream (InputStream EOFs
    // before the promised bytes are delivered).  Without this the peer
    // receives END_STREAM on a truncated body and waits forever for the
    // missing bytes (FUTURE-TIMEOUT on the HTTP client).
    int64_t content_length = -1;
    if (headers) {
        ConstHashIterator hi(headers);
        while (hi.next()) {
            if (!strcasecmp(hi.getKey(), "content-length")) {
                QoreValue v = hi.get();
                if (v.getType() == NT_STRING) {
                    const char* s = v.get<const QoreStringNode>()->c_str();
                    char* endptr = nullptr;
                    long long cl = strtoll(s, &endptr, 10);
                    if (endptr != s && cl >= 0) {
                        content_length = cl;
                    }
                } else if (v.getType() == NT_INT) {
                    content_length = v.getAsBigInt();
                }
                break;
            }
        }
    }

    // Transfer ownership of InputStream to the session for I/O thread reading
    // The handler thread unassigns before calling; I/O thread will reassign on first read
    body->unassignThread(xsink);
    if (*xsink) {
        return -1;
    }

    h2->setStreamInputStream(stream_id, body, xsink, content_length);
    return *xsink ? -1 : 0;
}

bool QoreSocketObject::isHttp2StreamClosed(int32_t stream_id) const {
    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    return !h2 || h2->isStreamClosed(stream_id);
}

bool QoreSocketObject::isHttp2StreamRemoteClosed(int32_t stream_id) const {
    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    return !h2 || h2->isStreamRemoteClosed(stream_id);
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
    // Look up the session briefly; the controller-backed wait must not hold
    // priv->m while the I/O thread drives sendPendingData().
    Http2SessionPtr h2 = qore_socket_object_get_h2_session(this);
    if (!h2) {
        return -1;
    }
    if (timeout_ms == 0) {
        return h2->waitForStreamDrain(stream_id, 0);
    }
    return qore_socket_object_exec_stream_drain(this,
        new QoreSocketObjectStreamDrainPollOperation(h2, stream_id), timeout_ms, "waitForHttp2StreamDrain",
        xsink);
}

long QoreSocketObject::verifyPeerCertificate() {
    AutoLocker al(priv->m);
    return priv->socket->verifyPeerCertificate();
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
    if (qore_on_async_io_thread()) {
        AutoLocker al(priv->m);
        return priv->socket->setNoDelay(nodelay);
    }

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
    if (qore_on_async_io_thread()) {
        AutoLocker al(priv->m);
        return priv->socket->getNoDelay();
    }

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
    if (qore_on_async_io_thread()) {
        AutoLocker al(priv->m);
        return priv->socket->setUserTimeout(ms);
    }

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
    if (qore_on_async_io_thread()) {
        AutoLocker al(priv->m);
        return priv->socket->getUserTimeout();
    }

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
    AutoLocker al(priv->m);
    priv->socket->setSslVerifyMode(mode);
}

int QoreSocketObject::getSslVerifyMode() const {
    AutoLocker al(priv->m);
    return priv->socket->getSslVerifyMode();
}

void QoreSocketObject::acceptAllCertificates(bool accept_all) {
    AutoLocker al(priv->m);
    priv->socket->acceptAllCertificates(accept_all);
}

bool QoreSocketObject::getAcceptAllCertificates() const {
    AutoLocker al(priv->m);
    return priv->socket->getAcceptAllCertificates();
}

bool QoreSocketObject::captureRemoteCertificates(bool set) {
    AutoLocker al(priv->m);
    return priv->socket->captureRemoteCertificates(set);
}

QoreObject* QoreSocketObject::getRemoteCertificate() const {
    AutoLocker al(priv->m);
    return priv->socket->getRemoteCertificate();
}

int64 QoreSocketObject::getConnectionId() const {
    AutoLocker al(priv->m);
    return priv->socket->getConnectionId();
}

void QoreSocketObject::setMaxChunkedBodySize(int64 size) {
    AutoLocker al(priv->m);
    priv->socket->setMaxChunkedBodySize(size);
}

int64 QoreSocketObject::getMaxChunkedBodySize() const {
    AutoLocker al(priv->m);
    return priv->socket->getMaxChunkedBodySize();
}

void QoreSocketObject::setHttp2MaxRequestBodySize(int64 size) {
    AutoLocker al(priv->m);
    priv->socket->setHttp2MaxRequestBodySize(size);
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

int64_t QoreSocketObject::submitQuicRequest(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len, ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_first_quic_session(this);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no active QUIC session on this socket; "
            "use startPollQuicConnect() first");
        return -1;
    }

    // Convert headers hash to std::map
    strcase_str_map_t hdr_map;
    if (headers) {
        ConstHashIterator hi(headers);
        while (hi.next()) {
            QoreStringValueHelper val(hi.get());
            hdr_map[hi.getKey()] = val->c_str();
        }
    }

    return session->submitRequest(method, path, hdr_map, body, body_len, xsink);
}

int64_t QoreSocketObject::submitQuicRequestStreaming(const char* method, const char* path,
        const QoreHashNode* headers, ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_first_quic_session(this);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no active QUIC session on this socket; "
            "use startPollQuicConnect() first");
        return -1;
    }

    // Convert headers hash to std::map
    strcase_str_map_t hdr_map;
    if (headers) {
        ConstHashIterator hi(headers);
        while (hi.next()) {
            QoreStringValueHelper val(hi.get());
            hdr_map[hi.getKey()] = val->c_str();
        }
    }

    return session->submitRequestStreaming(method, path, hdr_map, xsink);
}

int QoreSocketObject::sendQuicClientStreamData(int64_t stream_id, const void* data,
        size_t len, bool end_stream, ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_first_quic_session(this);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no active QUIC session on this socket");
        return -1;
    }

    return session->sendStreamData(stream_id, data, len, end_stream, xsink);
}

bool QoreSocketObject::isQuicSessionClosed() const {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_first_quic_session(this);
    if (!session) {
        return true;
    }
    return session->isClosed();
}

int QoreSocketObject::waitForQuicClientStreamDrain(int64_t stream_id, int timeout_ms,
        ExceptionSink* xsink) {
    SocketSyncPoll::assertNotOnIoThread("Socket", "waitForQuicClientStreamDrain", xsink);
    if (*xsink) {
        return -1;
    }
    std::shared_ptr<QuicSession> session = qore_socket_object_get_first_quic_session(this);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no active QUIC session on this socket");
        return -1;
    }

    if (timeout_ms == 0) {
        return session->waitForStreamDrain(stream_id, 0);
    }
    return qore_socket_object_exec_stream_drain(this,
        new QoreSocketObjectStreamDrainPollOperation(session, stream_id), timeout_ms,
        "waitForQuicClientStreamDrain", xsink);
}

void QoreSocketObject::cancelQuicStream(int64_t session_id, int64_t stream_id, ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return;
    }

    session->cancelStream(stream_id, NGHTTP3_H3_REQUEST_CANCELLED, xsink);
}

int QoreSocketObject::submitQuicResponse(int64_t session_id, int64_t stream_id, int status_code,
        const QoreHashNode* headers, const void* body, size_t body_len, ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return -1;
    }

    // Convert headers hash to std::map
    strcase_str_map_t hdr_map;
    if (headers) {
        ConstHashIterator hi(headers);
        while (hi.next()) {
            QoreStringValueHelper val(hi.get());
            hdr_map[hi.getKey()] = val->c_str();
        }
    }

    return session->submitResponse(stream_id, status_code, hdr_map, body, body_len, xsink);
}

int64_t QoreSocketObject::getFirstQuicSessionId(ExceptionSink* xsink) const {
    AutoLocker al(priv->m);
    qore_socket_private* sp = qore_socket_private::get(*priv->socket);
    return sp->getFirstQuicSessionId(xsink);
}

void QoreSocketObject::submitQuicShutdownNotice(int64_t session_id, ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        xsink->raiseException("QUIC-SESSION-ERROR", "session %lld not found",
                              (long long)session_id);
        return;
    }
    session->submitShutdownNotice(xsink);
}

void QoreSocketObject::submitQuicShutdown(int64_t session_id, ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        xsink->raiseException("QUIC-SESSION-ERROR", "session %lld not found",
                              (long long)session_id);
        return;
    }
    session->submitShutdown(xsink);
}

QoreHashNode* QoreSocketObject::getQuicSessionGoawayState(int64_t session_id, ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        xsink->raiseException("QUIC-SESSION-ERROR", "session %lld not found",
                              (long long)session_id);
        return nullptr;
    }
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclQuicGoawayStateInfo, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    h->setKeyValue("goaway_sent", session->isGoawaySent(), xsink);
    h->setKeyValue("goaway_received", session->isGoawayReceived(), xsink);
    h->setKeyValue("goaway_max_stream_id", session->getGoawayMaxStreamId(), xsink);
    return h.release();
}

bool QoreSocketObject::isQuicGoawayReceived(int64_t session_id, ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        // Return false rather than raising an exception for a lightweight check;
        // the session may have been cleaned up between checks
        return false;
    }
    return session->isGoawayReceived();
}

QoreObject* QoreSocketObject::getQuicPeerCertificate(int64_t session_id, ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        xsink->raiseException("QUIC-SESSION-ERROR",
            "no QUIC session with id %lld on this socket", (long long)session_id);
        return nullptr;
    }
    // getPeerCertificate() returns X509* with refcount incremented (SSL_get_peer_certificate);
    // QoreSSLCertificate takes ownership
    X509* cert = session->getPeerCertificate();
    if (!cert) {
        return nullptr;
    }
    return new QoreObject(QC_SSLCERTIFICATE, getProgram(), new QoreSSLCertificate(cert));
}

int QoreSocketObject::submitQuicResponseStreaming(int64_t session_id, int64_t stream_id,
        int status_code, const QoreHashNode* headers, ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return -1;
    }

    // Convert headers hash to std::map
    strcase_str_map_t hdr_map;
    if (headers) {
        ConstHashIterator hi(headers);
        while (hi.next()) {
            QoreStringValueHelper val(hi.get());
            hdr_map[hi.getKey()] = val->c_str();
        }
    }

    return session->submitResponseStreaming(stream_id, status_code, hdr_map, xsink);
}

int QoreSocketObject::submitQuicStreamData(int64_t session_id, int64_t stream_id,
        const void* data, size_t len, bool end_stream, ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return -1;
    }

    return session->sendStreamData(stream_id, data, len, end_stream, xsink);
}

void QoreSocketObject::setQuicStreamInputStream(int64_t session_id, int64_t stream_id,
        InputStream* body, ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return;
    }

    // Unassign InputStream from handler thread so I/O thread can claim it
    body->unassignThread(xsink);
    if (*xsink) {
        return;
    }

    session->setStreamInputStream(stream_id, body, xsink);
}

int QoreSocketObject::waitForQuicStreamDrain(int64_t session_id, int64_t stream_id,
        int timeout_ms, ExceptionSink* xsink) {
    SocketSyncPoll::assertNotOnIoThread("Socket", "waitForQuicStreamDrain", xsink);
    if (*xsink) {
        return -1;
    }
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return -1;
    }
    if (timeout_ms == 0) {
        return session->waitForStreamDrain(stream_id, 0);
    }
    return qore_socket_object_exec_stream_drain(this,
        new QoreSocketObjectStreamDrainPollOperation(session, stream_id), timeout_ms,
        "waitForQuicStreamDrain", xsink);
}

int QoreSocketObject::submitQuicConnectResponse(int64_t session_id, int64_t stream_id,
        int status_code, const QoreHashNode* headers, ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return -1;
    }

    // Convert headers hash to std::map
    strcase_str_map_t hdr_map;
    if (headers) {
        ConstHashIterator hi(headers);
        while (hi.next()) {
            QoreStringValueHelper val(hi.get());
            hdr_map[hi.getKey()] = val->c_str();
        }
    }

    return session->submitConnectResponse(stream_id, status_code, hdr_map, xsink);
}

QoreValue QoreSocketObject::readQuicConnectStreamData(int64_t session_id, int64_t stream_id,
        ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return QoreValue();
    }

    return session->readConnectStreamData(stream_id, xsink);
}

void QoreSocketObject::registerQuicConnectStreamQueue(int64_t session_id, int64_t stream_id,
        Queue* queue, ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return;
    }
    session->registerConnectStreamQueue(stream_id, queue);
}

void QoreSocketObject::deregisterQuicConnectStreamQueue(int64_t session_id, int64_t stream_id,
        ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        return;  // silently ignore — session may already be gone during cleanup
    }
    session->deregisterConnectStreamQueue(stream_id);
}

void QoreSocketObject::registerQuicConnectStreamFrameState(int64_t session_id, int64_t stream_id,
        Queue* msg_queue, ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        ExceptionSink tmp;
        msg_queue->deref(&tmp);
        tmp.clear();
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return;
    }
    session->registerConnectStreamFrameState(stream_id, msg_queue);
}

void QoreSocketObject::deregisterQuicConnectStreamFrameState(int64_t session_id, int64_t stream_id,
        ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        return;
    }
    session->deregisterConnectStreamFrameState(stream_id);
}

BinaryNode* QoreSocketObject::readQuicStreamDataBlock(int64_t session_id, int64_t stream_id,
        int timeout_ms, ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return nullptr;
    }
    return qore_socket_object_exec_quic_stream_data(this,
        new QoreSocketObjectQuicStreamDataPollOperation(session, session_id, stream_id), timeout_ms,
        session_id, stream_id, xsink);
}

bool QoreSocketObject::isQuicStreamComplete(int64_t session_id, int64_t stream_id) const {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        return true;  // Session not found, treat as complete
    }
    return session->isStreamComplete(stream_id);
}

void QoreSocketObject::cleanupQuicStream(int64_t session_id, int64_t stream_id,
        ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return;
    }
    session->cleanupStream(stream_id);
}

int QoreSocketObject::resetQuicStream(int64_t session_id, int64_t stream_id,
        ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return -1;
    }
    return session->resetStream(stream_id);
}

int QoreSocketObject::submitQuicDatagram(int64_t session_id, int64_t stream_id,
        const BinaryNode* data, ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return -1;
    }
    const uint8_t* ptr = data ? reinterpret_cast<const uint8_t*>(data->getPtr()) : nullptr;
    size_t len = data ? data->size() : 0;
    return session->submitDatagram(stream_id, ptr, len, xsink);
}

QoreValue QoreSocketObject::readQuicDatagram(int64_t session_id, int64_t stream_id,
        int timeout_ms, ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return QoreValue();
    }
    return qore_socket_object_exec_quic_datagram(this,
        new QoreSocketObjectQuicDatagramPollOperation(session, stream_id), timeout_ms, xsink);
}

void QoreSocketObject::registerQuicDatagramQueue(int64_t session_id, int64_t stream_id,
        Queue* queue, ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return;
    }
    session->registerDatagramQueue(stream_id, queue, xsink);
}

void QoreSocketObject::unregisterQuicDatagramQueue(int64_t session_id, int64_t stream_id,
        ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        // Session gone — nothing to unregister.  Silent for idempotent teardown.
        return;
    }
    session->unregisterDatagramQueue(stream_id, xsink);
}

int64_t QoreSocketObject::getQuicMaxDatagramSize(int64_t session_id, int64_t stream_id,
        ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return 0;
    }
    return static_cast<int64_t>(session->getMaxDatagramPayloadSize(stream_id));
}

bool QoreSocketObject::isQuicDatagramSupported(int64_t session_id, ExceptionSink* xsink) {
    std::shared_ptr<QuicSession> session = qore_socket_object_get_quic_session(this, session_id);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return false;
    }
    return session->isDatagramSupported();
}

bool my_socket_priv::hasQuicSession() const {
    qore_socket_private* sp = qore_socket_private::get(*socket);
    AutoLocker al(sp->quic_sessions_lock);
    return !sp->quic_sessions.empty();
}

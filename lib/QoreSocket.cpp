/* -*- indent-tabs-mode: nil -*- */
/*
    QoreSocket.cpp

    Socket Class for IPv4, IPv6 and UNIX domain sockets with SSL support

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

// FIXME: change int to size_t where applicable! (ex: int rc = recv())

#include <cerrno>
#include <cctype>
#include <climits>
#include <algorithm>
#include <atomic>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <qore/Qore.h>
#include <qore/QoreSocket.h>
#include <qore/QoreSocketObject.h>
#include <qore/QoreSSLCertificate.h>
#include <qore/Transform.h>

#include "qore/intern/QC_Socket.h"
#include "qore/intern/QC_SocketPollOperation.h"
#include "qore/intern/AsyncIoControllerPriv.h"
#include "qore/intern/FileInputStream.h"
#include "qore/intern/FileOutputStream.h"
#include "qore/intern/QC_Queue.h"
#include "qore/intern/QoreAsyncIoLogger.h"
#include "qore/intern/QoreEventNotifier.h"
#include "qore/intern/qore_socket_private.h"
#include "qore/intern/SocketSyncPoll.h"
#include "qore/intern/AsyncCompletionAction.h"
#include "qore/intern/qore_string_private.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/CompressionTransforms.h"
#include "qore/intern/QoreLibIntern.h"

#include <ares.h>

// maximum number of non-blocking network operations before returning
constexpr unsigned max_nonblock_ops = 32;

static std::atomic<uint64_t> qore_socket_sync_exec_seq{0};

extern qore_classid_t CID_ASYNCIOCONTROLLER;
extern QoreClass* QC_EVENTNOTIFIER;

static int qore_socket_close_private_from_controller(qore_socket_private* priv);

static QoreSandboxManager* qore_socket_ref_current_sandbox_manager() {
    QoreSandboxManagerHelper smh;
    if (!smh) {
        return nullptr;
    }

    QoreSandboxManager* sm = smh.get();
    sm->ref();
    return sm;
}

static QoreSandboxManager* qore_socket_ref_sandbox_manager(QoreSandboxManager* sm) {
    if (!sm) {
        return qore_socket_ref_current_sandbox_manager();
    }

    sm->ref();
    return sm;
}

#ifdef _Q_WINDOWS
static int qore_windows_set_errno(int rc);
#endif

static int qore_input_stream_pollable_descriptor(InputStream* is, const char* err, ExceptionSink* xsink) {
    if (!is->supportsNonBlockingIo()) {
        return -1;
    }

    int fd = is->getPollableDescriptor();
    if (fd < 0) {
        xsink->raiseException(err, "InputStream reports non-blocking I/O support but returned no pollable descriptor");
    }
    return fd;
}

static int qore_output_stream_pollable_descriptor(OutputStream* os, const char* err, ExceptionSink* xsink) {
    if (!os->supportsNonBlockingIo()) {
        return -1;
    }

    int fd = os->getPollableDescriptor();
    if (fd < 0) {
        xsink->raiseException(err, "OutputStream reports non-blocking I/O support but returned no pollable descriptor");
    }
    return fd;
}

void se_in_op(const char* cname, const char* meth, ExceptionSink* xsink) {
    assert(xsink);
    xsink->raiseException("SOCKET-IN-CALLBACK", "calls to %s::%s() cannot be made from a callback on an operation on "
        "the same socket", cname, meth);
}

void se_in_op_thread(const char* cname, const char* meth, ExceptionSink* xsink) {
    assert(xsink);
    xsink->raiseException("SOCKET-IN-CALLBACK", "calls to %s::%s() cannot be made from another thread while a "
        "callback operation is in progress on the same socket", cname, meth);
}

void se_not_open(const char* cname, const char* meth, ExceptionSink* xsink, const char* extra) {
    assert(xsink);
    QoreStringNode* desc = new QoreStringNodeMaker("socket must be opened before %s::%s() call", cname, meth);
    if (extra) {
        desc->sprintf(" (%s)", extra);
    }
    xsink->raiseException("SOCKET-NOT-OPEN", desc);
}

void se_timeout(const char* cname, const char* meth, int timeout_ms, ExceptionSink* xsink, const char* extra) {
    assert(xsink);
    QoreStringNodeHolder desc(new QoreStringNodeMaker("timed out after %d millisecond%s in %s::%s() call", timeout_ms,
        timeout_ms == 1 ? "" : "s", cname, meth));
    if (extra) {
        desc->sprintf(" (%s)", extra);
    }
    xsink->raiseException("SOCKET-TIMEOUT", desc.release());
}

void se_closed(const char* cname, const char* mname, ExceptionSink* xsink) {
    assert(xsink);
    xsink->raiseException("SOCKET-CLOSED", "error in %s::%s(): remote end closed the connection", cname, mname);
}

static int qore_socket_plain_data_available(qore_socket_private* priv, const char* mname, ExceptionSink* xsink) {
    unsigned loop = 0;
    while (true) {
        char c;
        ssize_t rc = ::recv(priv->sock, &c, 1, MSG_PEEK | MSG_DONTWAIT);
        if (rc > 0) {
            return 1;
        }
        if (rc == 0) {
            qore_socket_close_private_from_controller(priv);
            se_closed("Socket", mname, xsink);
            return -1;
        }

        int e = sock_get_error();
        if (e == EINTR) {
            if (++loop >= max_nonblock_ops) {
                return 0;
            }
            continue;
        }
        if (e == EAGAIN
#ifdef EWOULDBLOCK
                || e == EWOULDBLOCK
#endif
                ) {
            return 0;
        }

        xsink->raiseErrnoException("SOCKET-RECV-ERROR", e, "error while executing Socket::%s()", mname);
        return -1;
    }
}

static bool qore_socket_is_accepting(qore_socket_private* priv) {
#ifdef SO_ACCEPTCONN
    int val = 0;
    socklen_t len = sizeof(val);
    if (!getsockopt(priv->sock, SOL_SOCKET, SO_ACCEPTCONN, (GETSOCKOPT_ARG_4)&val, &len)) {
        return val || priv->listening;
    }
#endif
    return priv->listening;
}

static int qore_socket_poll_fd_revents_now(int fd, short events, short& revents, const char* err, const char* desc,
        ExceptionSink* xsink) {
    unsigned loop = 0;
    while (true) {
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = events;
        pfd.revents = 0;

        int rc = ::poll(&pfd, 1, 0);
        if (rc >= 0) {
            revents = rc > 0 ? pfd.revents : 0;
            return rc > 0 ? 1 : 0;
        }
        int e = errno;
        if (e == EINTR && ++loop < max_nonblock_ops) {
            continue;
        }
        revents = 0;
        xsink->raiseException(err, "poll() on %s fd failed: %s", desc, strerror(e));
        return -1;
    }
}

static int qore_socket_poll_fd_now(int fd, short events, const char* err, const char* desc, ExceptionSink* xsink) {
    short revents;
    return qore_socket_poll_fd_revents_now(fd, events, revents, err, desc, xsink);
}

static int qore_socket_set_raw_error(int rc) {
#ifdef _Q_WINDOWS
    WSASetLastError(rc);
    return qore_windows_set_errno(rc);
#else
    errno = rc;
    return errno;
#endif
}

// returns 0 = connected, 1 = still in progress, -1 = error
static int qore_socket_check_connect_ready(int fd) {
    int val = 0;
    socklen_t lon = sizeof(val);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (GETSOCKOPT_ARG_4)&val, &lon) == QORE_SOCKET_ERROR) {
        sock_get_error();
        return -1;
    }

    if (val) {
        int e = qore_socket_set_raw_error(val);
        if (e == EINPROGRESS || e == EAGAIN || e == ENOTCONN) {
            return 1;
        }
        return -1;
    }

    // Try a zero-byte send to confirm connection.  This is needed on macOS,
    // where poll() may report writable readiness before connect completion.
    int rc = ::send(fd, nullptr, 0, 0);
    if (rc) {
        int e = sock_get_error();
        if (e == EINPROGRESS || e == EAGAIN || e == ENOTCONN) {
            return 1;
        }
        return -1;
    }

    return 0;
}

static void qore_socket_raise_poll_result_exception(const QoreHashNode* ex, ExceptionSink* xsink) {
    QoreValue err = ex->getKeyValue("err");
    QoreValue desc = ex->getKeyValue("desc");
    QoreValue arg = ex->getKeyValue("arg");
    QoreStringValueHelper err_str(err);
    QoreStringValueHelper desc_str(desc);
    xsink->raiseException(
        err.getType() == NT_STRING && err_str
            ? new QoreStringNode(**err_str)
            : new QoreStringNode("ASYNC-IO-ERROR"),
        desc.getType() == NT_STRING && desc_str
            ? new QoreStringNode(**desc_str)
            : new QoreStringNode("async socket operation failed"),
        arg.refSelf());
}

static bool qore_socket_exec_exception_is(const QoreHashNode& ex, const char* err) {
    QoreValue ex_err = ex.getKeyValue("err");
    QoreStringValueHelper ex_err_str(ex_err);
    return ex_err.getType() == NT_STRING && ex_err_str && !strcmp(ex_err_str->c_str(), err);
}

static int qore_socket_private_check_async_sequence_allowed_intern(const qore_socket_private& priv,
        ExceptionSink* xsink, unsigned direction, int tid) {
    int index = my_socket_priv::getAsyncSequenceIndex(direction);
    if (priv.async_sequence_count[index] && priv.async_sequence_owner_tid[index] != tid) {
        xsink->raiseException("SOCKET-ASYNC-MODE-ERROR",
            "cannot start a controller-backed %s operation while TID %d owns a controller-backed %s operation "
            "on the socket",
            my_socket_priv::getNonBlockDirectionName(direction), priv.async_sequence_owner_tid[index],
            my_socket_priv::getNonBlockDirectionName(direction));
        return -1;
    }
    return 0;
}

int qore_socket_private::checkAsyncSequenceAllowedForTid(ExceptionSink* xsink, unsigned direction, int tid) const {
    AutoLocker al(async_sequence_m);

    unsigned flags = direction & NB_ALL;
    if ((flags & NB_SEND) && qore_socket_private_check_async_sequence_allowed_intern(*this, xsink, NB_SEND, tid)) {
        return -1;
    }
    if ((flags & NB_RECV) && qore_socket_private_check_async_sequence_allowed_intern(*this, xsink, NB_RECV, tid)) {
        return -1;
    }
    if ((flags & NB_CONNECT)
            && qore_socket_private_check_async_sequence_allowed_intern(*this, xsink, NB_CONNECT, tid)) {
        return -1;
    }
    return 0;
}

static void qore_socket_private_start_async_sequence_io_intern(qore_socket_private& priv, unsigned direction,
        int tid) {
    int index = my_socket_priv::getAsyncSequenceIndex(direction);
    assert(!priv.async_sequence_count[index] || priv.async_sequence_owner_tid[index] == tid);
    if (!priv.async_sequence_count[index]) {
        priv.async_sequence_owner_tid[index] = tid;
    }
    ++priv.async_sequence_count[index];
}

int qore_socket_private::startAsyncSequenceIo(ExceptionSink* xsink, unsigned direction) {
    AutoLocker al(async_sequence_m);

    int tid = q_gettid();
    unsigned flags = direction & NB_ALL;
    if ((flags & NB_SEND) && qore_socket_private_check_async_sequence_allowed_intern(*this, xsink, NB_SEND, tid)) {
        return -1;
    }
    if ((flags & NB_RECV) && qore_socket_private_check_async_sequence_allowed_intern(*this, xsink, NB_RECV, tid)) {
        return -1;
    }
    if ((flags & NB_CONNECT)
            && qore_socket_private_check_async_sequence_allowed_intern(*this, xsink, NB_CONNECT, tid)) {
        return -1;
    }

    if (flags & NB_SEND) {
        qore_socket_private_start_async_sequence_io_intern(*this, NB_SEND, tid);
    }
    if (flags & NB_RECV) {
        qore_socket_private_start_async_sequence_io_intern(*this, NB_RECV, tid);
    }
    if (flags & NB_CONNECT) {
        qore_socket_private_start_async_sequence_io_intern(*this, NB_CONNECT, tid);
    }
    return 0;
}

static void qore_socket_private_clear_async_sequence_io_intern(qore_socket_private& priv, unsigned direction,
        int tid) {
    int index = my_socket_priv::getAsyncSequenceIndex(direction);
    assert(priv.async_sequence_count[index] > 0);
    assert(priv.async_sequence_owner_tid[index] == tid);
    --priv.async_sequence_count[index];
    if (!priv.async_sequence_count[index]) {
        priv.async_sequence_owner_tid[index] = -1;
    }
}

void qore_socket_private::clearAsyncSequenceIo(unsigned direction) {
    AutoLocker al(async_sequence_m);

    int tid = q_gettid();
    unsigned flags = direction & NB_ALL;
    if (flags & NB_SEND) {
        qore_socket_private_clear_async_sequence_io_intern(*this, NB_SEND, tid);
    }
    if (flags & NB_RECV) {
        qore_socket_private_clear_async_sequence_io_intern(*this, NB_RECV, tid);
    }
    if (flags & NB_CONNECT) {
        qore_socket_private_clear_async_sequence_io_intern(*this, NB_CONNECT, tid);
    }
}

class QoreSocketRawAsyncIoGuard {
public:
    DLLLOCAL QoreSocketRawAsyncIoGuard(qore_socket_private& priv, ExceptionSink* xsink, unsigned direction)
            : priv(priv), direction(direction & NB_ALL) {
        SocketSyncPoll::assertNotOnIoThread("Socket", "sync", xsink);
        if (qore_on_async_io_thread() || (xsink && *xsink)) {
            return;
        }
        active = this->direction && !priv.startAsyncSequenceIo(xsink, this->direction);
    }

    DLLLOCAL ~QoreSocketRawAsyncIoGuard() {
        if (active) {
            priv.clearAsyncSequenceIo(direction);
        }
    }

    DLLLOCAL explicit operator bool() const {
        return active;
    }

private:
    qore_socket_private& priv;
    unsigned direction;
    bool active = false;
};

static bool qore_socket_parse_bind_name(const char* bind_name, std::string& host, std::string& service, int& family);
static int qore_socket_bind_unix_direct(QoreSocket* s, const char* name, int socktype, int protocol,
        ExceptionSink* xsink);
static int qore_socket_bind_sockaddr_direct(QoreSocket* s, const struct sockaddr* addr, int size,
        ExceptionSink* xsink);
static int qore_socket_bind_family_sockaddr_direct(QoreSocket* s, int family, const struct sockaddr* addr,
        int size, int sock_type, int protocol, ExceptionSink* xsink);
static int qore_socket_listen_direct(QoreSocket* s, int backlog);
static int qore_socket_shutdown_direct(QoreSocket* s);
static int qore_socket_set_no_delay_direct(QoreSocket* s, int nodelay);
static int qore_socket_get_no_delay_direct(QoreSocket* s);
static int qore_socket_set_user_timeout_direct(QoreSocket* s, int ms);
static int qore_socket_get_user_timeout_direct(QoreSocket* s);
static int qore_socket_set_socket_timeout_direct(QoreSocket* s, int optname, int ms);
static int qore_socket_get_socket_timeout_direct(QoreSocket* s, int optname);
static int qore_socket_get_port_direct(QoreSocket* s);
static bool qore_socket_exec_process_sse_char(qore_socket_private* priv, QoreString& str, int& eol_count, char c);

static int qore_socket_close_private_from_controller(qore_socket_private* priv) {
    priv->prepareForClose();
    return priv->close();
}

static int qore_socket_close_from_controller(QoreSocket* s) {
    return qore_socket_close_private_from_controller(qore_socket_private::get(*s));
}

static void qore_socket_wake_async_controller(qore_socket_private* priv) {
    if (qore_on_async_io_thread()) {
        return;
    }

    ExceptionSink xsink;
    ReferenceHolder<QoreObject> ctl_obj(qore_get_async_io_controller_obj(&xsink), &xsink);
    ReferenceHolder<AsyncIoControllerPriv> ctrl(
        !xsink && ctl_obj
            ? static_cast<AsyncIoControllerPriv*>(
                (*ctl_obj)->getReferencedPrivateData(CID_ASYNCIOCONTROLLER, &xsink))
            : nullptr,
        &xsink);
    if (!xsink && ctrl) {
        QoreString tmp;
        qore_get_ptr_hash(tmp, priv);
        ctrl->wakeSocket(tmp.c_str());
    }
    xsink.clear();
}

class QoreSocketControllerPollable : public AbstractPollableIoObjectBase {
public:
    DLLLOCAL QoreSocketControllerPollable(QoreSocket* sock, bool use_outer_lock = true)
            : QoreSocketControllerPollable(qore_socket_private::get(*sock), use_outer_lock) {
    }

    DLLLOCAL QoreSocketControllerPollable(qore_socket_private* priv, bool use_outer_lock = true)
            : priv(priv), close_lock(use_outer_lock ? priv->outer_lock : nullptr) {
        priv->ref();
        QoreString tmp;
        qore_get_ptr_hash(tmp, priv);
        identity_hash = tmp.c_str();
    }

    DLLLOCAL ~QoreSocketControllerPollable() {
        priv->deref();
    }

    DLLLOCAL virtual int getPollableDescriptor() const override {
        return priv->sock;
    }

    DLLLOCAL virtual const std::string& getIoIdentityHash() const override {
        return identity_hash;
    }

    DLLLOCAL virtual void closeIo(ExceptionSink*) override {
        priv->prepareForClose();
        if (close_lock) {
            AutoLocker al(*close_lock);
            closeIoLocked();
        } else {
            closeIoLocked();
        }
    }

#ifdef DARWIN
    DLLLOCAL virtual void setPollNotifyFd(int fd) override {
        priv->poll_notify_fd.store(fd, std::memory_order_release);
    }
#endif

    DLLLOCAL virtual bool hasPendingData() const override {
        if (priv->buflen) {
            return true;
        }
        if (priv->ssl && priv->ssl->pending() > 0) {
            return true;
        }
        return priv->h2_session && (priv->h2_session->hasStreamData() || priv->h2_session->wantWrite());
    }

private:
    DLLLOCAL void closeIoLocked() {
        if (priv->isOpen()) {
            priv->shutdown_direct();
            priv->close();
        }
    }

    qore_socket_private* priv;
    QoreThreadLock* close_lock;
    std::string identity_hash;
};

class QoreSocketControllerDataAvailablePollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketControllerDataAvailablePollOperation(QoreSocket* sock) : sock(sock) {
    }

    DLLLOCAL virtual bool goalReached() const override {
        return ready;
    }

    DLLLOCAL virtual void abort(ExceptionSink*) override {
        waiting = false;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        qore_socket_private* priv = qore_socket_private::get(*sock);

        if (ready) {
            return nullptr;
        }
        if (priv->sock == QORE_INVALID_SOCKET) {
            se_not_open("Socket", "isDataAvailable", xsink);
            return nullptr;
        }
        if (priv->buflen) {
            ready = true;
            return nullptr;
        }

        int32_t h2_active_stream_id = priv->getH2ActiveStreamId();
        if (priv->h2_session && priv->h2_session->isServer() && h2_active_stream_id > 0
                && !priv->h2_receiving_frames) {
            xsink->raiseException("SOCKET-H2-SYNC-ERROR",
                "Socket::isDataAvailable() is not supported on an HTTP/2-active "
                "server socket; use HttpServerAsyncIo + register_body_queue "
                "for inbound DATA");
            return nullptr;
        }

        if (priv->ssl) {
            char c;
            size_t real_io = 0;
            OptionalNonBlockingHelper nbh(*priv, true, xsink);
            if (*xsink) {
                return nullptr;
            }
            int rc = priv->ssl->doNonBlockingIo(xsink, "isDataAvailable", &c, 1, PEEK, real_io);
            if (*xsink) {
                return nullptr;
            }
            if (!rc) {
                ready = real_io > 0;
                return nullptr;
            }
            return getSocketPollInfoHash(xsink, rc);
        }

        if (qore_socket_is_accepting(priv)) {
            if (!waiting) {
                waiting = true;
                return getSocketPollInfoHash(xsink, SOCK_POLLIN);
            }

            ready = true;
            return nullptr;
        }

        int data_available = qore_socket_plain_data_available(priv, "isDataAvailable", xsink);
        if (*xsink) {
            return nullptr;
        }
        if (data_available > 0) {
            ready = true;
            return nullptr;
        }

        waiting = true;
        return getSocketPollInfoHash(xsink, SOCK_POLLIN);
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return ready;
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        return ready ? "data-available" : "waiting-read";
    }

private:
    QoreSocket* sock;
    bool waiting = false;
    bool ready = false;
};

class QoreSocketControllerReadinessPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketControllerReadinessPollOperation(QoreSocket* sock, int events, const char* method_name,
            const char* waiting_state, const char* ready_state) : sock(sock), events(events),
            method_name(method_name), waiting_state(waiting_state), ready_state(ready_state) {
    }

    DLLLOCAL virtual bool goalReached() const override {
        return ready;
    }

    DLLLOCAL virtual void abort(ExceptionSink*) override {
        waiting = false;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (ready) {
            return nullptr;
        }

        qore_socket_private* priv = qore_socket_private::get(*sock);
        if (priv->sock == QORE_INVALID_SOCKET) {
            se_not_open("Socket", method_name, xsink);
            return nullptr;
        }
        if (!waiting) {
            waiting = true;
            return getSocketPollInfoHash(xsink, events);
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
    QoreSocket* sock;
    int events;
    const char* method_name;
    const char* waiting_state;
    const char* ready_state;
    bool waiting = false;
    bool ready = false;
};

class QoreSocketHttp2StreamDrainPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketHttp2StreamDrainPollOperation(Http2SessionPtr h2, int32_t stream_id)
            : h2(std::move(h2)), stream_id(stream_id) {
    }

    DLLLOCAL QoreSocketHttp2StreamDrainPollOperation(QoreSocket* sock, int32_t stream_id)
            : sock(sock), stream_id(stream_id) {
    }

    DLLLOCAL ~QoreSocketHttp2StreamDrainPollOperation() override {
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
        if (h2) {
            return true;
        }
        assert(sock);
        qore_socket_private* priv = qore_socket_private::get(*sock);
        h2 = priv->h2_session;
        if (!h2) {
            xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
            return false;
        }
        return true;
    }

    DLLLOCAL int checkDrain(ExceptionSink* xsink) {
        if (!ensureSession(xsink)) {
            return -1;
        }
        return h2->waitForStreamDrain(stream_id, 0);
    }

    DLLLOCAL int registerWaiter(ExceptionSink* xsink) {
        if (!ensureSession(xsink)) {
            return -1;
        }
        ReferenceHolder<QoreObject> obj(getReferencedSocketObject(xsink), xsink);
        if (*xsink) {
            return -1;
        }
        h2->registerStreamDrainWaiter(*obj, xsink);
        if (*xsink) {
            return -1;
        }
        waiter_sock_obj = obj.release();
        registered = true;
        return 0;
    }

    DLLLOCAL void cleanup(ExceptionSink* xsink) {
        if (registered && waiter_sock_obj) {
            h2->unregisterStreamDrainWaiter(waiter_sock_obj, xsink);
            registered = false;
        }
        if (waiter_sock_obj) {
            waiter_sock_obj->deref(xsink);
            waiter_sock_obj = nullptr;
        }
    }

    QoreSocket* sock = nullptr;
    Http2SessionPtr h2;
    QoreObject* waiter_sock_obj = nullptr;
    int32_t stream_id;
    int output = -1;
    bool done = false;
    bool registered = false;
    bool wake_requested = false;
};

static QoreHashNode* qore_socket_make_sockaddr_output(const struct sockaddr_storage& addr, socklen_t len,
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

class QoreSocketControllerAddressInfoPollOperation : public SocketPollOperationBase {
public:
    enum class Action {
        Peer,
        Socket,
    };

    DLLLOCAL QoreSocketControllerAddressInfoPollOperation(QoreSocket* sock, Action action, bool host_lookup)
            : sock(sock), action(action), host_lookup(host_lookup) {
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
            qore_socket_private* priv = qore_socket_private::get(*sock);
            int rc = action == Action::Peer
                ? priv->getPeerSockAddr(xsink, addr, len)
                : priv->getSocketSockAddr(xsink, addr, len);
            if (rc) {
                done = true;
                return nullptr;
            }
            socketname = priv->socketname;
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
        return success ? qore_socket_make_sockaddr_output(addr, len, socketname, hostname) : QoreValue();
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

    QoreSocket* sock;
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

class QoreSocketControllerSetupPollOperation : public SocketPollOperationBase {
private:
    enum class Action {
        BindPort,
        BindInterfacePort,
        BindUnix,
        BindInet,
        BindSockaddr,
        BindFamilySockaddr,
        Listen,
        Shutdown,
        SetNoDelay,
        GetNoDelay,
        SetUserTimeout,
        GetUserTimeout,
        SetSendTimeout,
        SetRecvTimeout,
        GetSendTimeout,
        GetRecvTimeout,
        GetPort,
        SetMaxChunkedBodySize,
        SetSslVerifyMode,
        GetSslVerifyMode,
        SetAcceptAllCertificates,
        GetAcceptAllCertificates,
        CaptureRemoteCertificates,
        SetHttp2MaxRequestBodySize,
    };

public:
    enum class ConfigAction {
        SetNoDelay,
        GetNoDelay,
        SetUserTimeout,
        GetUserTimeout,
        SetSendTimeout,
        SetRecvTimeout,
        GetSendTimeout,
        GetRecvTimeout,
        GetPort,
        SetMaxChunkedBodySize,
        SetSslVerifyMode,
        GetSslVerifyMode,
        SetAcceptAllCertificates,
        GetAcceptAllCertificates,
        CaptureRemoteCertificates,
        SetHttp2MaxRequestBodySize,
    };

    DLLLOCAL QoreSocketControllerSetupPollOperation(QoreSocket* sock, const char* name, bool reuseaddr)
            : sock(sock), action(Action::BindUnix), name(name), has_name(true), reuseaddr(reuseaddr),
            socktype(SOCK_STREAM) {
        if (qore_socket_parse_bind_name(name, this->name, service, family)) {
            action = Action::BindInet;
            has_service = true;
        }
    }

    DLLLOCAL QoreSocketControllerSetupPollOperation(QoreSocket* sock, int port, bool reuseaddr)
            : sock(sock), action(Action::BindPort), reuseaddr(reuseaddr), port(port) {
        service = std::to_string(port);
        has_service = true;
        socktype = SOCK_STREAM;
    }

    DLLLOCAL QoreSocketControllerSetupPollOperation(QoreSocket* sock, const char* iface, int port, bool reuseaddr)
            : sock(sock), action(Action::BindInterfacePort), name(iface), reuseaddr(reuseaddr), port(port) {
        has_name = true;
        service = std::to_string(port);
        has_service = true;
        socktype = SOCK_STREAM;
    }

    DLLLOCAL QoreSocketControllerSetupPollOperation(QoreSocket* sock, const char* name, int socktype, int protocol)
            : sock(sock), action(Action::BindUnix), name(name), socktype(socktype), protocol(protocol) {
    }

    DLLLOCAL QoreSocketControllerSetupPollOperation(QoreSocket* sock, const char* name, const char* service,
            bool reuseaddr, int family, int socktype, int protocol)
            : sock(sock), action(Action::BindInet), name(name ? name : ""), service(service ? service : ""),
            reuseaddr(reuseaddr), family(family), socktype(socktype), protocol(protocol) {
        has_name = name;
        has_service = service;
    }

    DLLLOCAL QoreSocketControllerSetupPollOperation(QoreSocket* sock, const struct sockaddr* addr, int size,
            ExceptionSink* xsink)
            : sock(sock), action(Action::BindSockaddr), addr_size(size) {
        copyAddress(addr, size, xsink);
    }

    DLLLOCAL QoreSocketControllerSetupPollOperation(QoreSocket* sock, int family, const struct sockaddr* addr,
            int size, int socktype, int protocol, ExceptionSink* xsink)
            : sock(sock), action(Action::BindFamilySockaddr), family(family), socktype(socktype), protocol(protocol),
            addr_size(size) {
        copyAddress(addr, size, xsink);
    }

    DLLLOCAL QoreSocketControllerSetupPollOperation(QoreSocket* sock, int backlog)
            : sock(sock), action(Action::Listen), backlog(backlog) {
    }

    DLLLOCAL QoreSocketControllerSetupPollOperation(QoreSocket* sock)
            : sock(sock), action(Action::Shutdown) {
    }

    DLLLOCAL QoreSocketControllerSetupPollOperation(QoreSocket* sock, ConfigAction config_action, int64 value = 0)
            : sock(sock), action(getAction(config_action)), value(value) {
    }

    DLLLOCAL ~QoreSocketControllerSetupPollOperation() override;

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

        switch (action) {
            case Action::BindPort:
                return continueBindInet(xsink);
            case Action::BindInterfacePort:
                return continueBindInet(xsink);
            case Action::BindUnix:
                rc = qore_socket_bind_unix_direct(sock, name.c_str(), socktype, protocol, xsink);
                break;
            case Action::BindInet:
                return continueBindInet(xsink);
            case Action::BindSockaddr:
                rc = qore_socket_bind_sockaddr_direct(sock, reinterpret_cast<const struct sockaddr*>(&addr),
                    addr_size, xsink);
                break;
            case Action::BindFamilySockaddr:
                rc = qore_socket_bind_family_sockaddr_direct(sock, family,
                    reinterpret_cast<const struct sockaddr*>(&addr), addr_size, socktype, protocol, xsink);
                break;
            case Action::Listen:
                rc = qore_socket_listen_direct(sock, backlog);
                break;
            case Action::Shutdown:
                rc = qore_socket_shutdown_direct(sock);
                break;
            case Action::SetNoDelay:
                rc = qore_socket_set_no_delay_direct(sock, static_cast<int>(value));
                break;
            case Action::GetNoDelay:
                rc = qore_socket_get_no_delay_direct(sock);
                break;
            case Action::SetUserTimeout:
                rc = qore_socket_set_user_timeout_direct(sock, static_cast<int>(value));
                break;
            case Action::GetUserTimeout:
                rc = qore_socket_get_user_timeout_direct(sock);
                break;
            case Action::SetSendTimeout:
                rc = qore_socket_set_socket_timeout_direct(sock, SO_SNDTIMEO, static_cast<int>(value));
                break;
            case Action::SetRecvTimeout:
                rc = qore_socket_set_socket_timeout_direct(sock, SO_RCVTIMEO, static_cast<int>(value));
                break;
            case Action::GetSendTimeout:
                rc = qore_socket_get_socket_timeout_direct(sock, SO_SNDTIMEO);
                break;
            case Action::GetRecvTimeout:
                rc = qore_socket_get_socket_timeout_direct(sock, SO_RCVTIMEO);
                break;
            case Action::GetPort:
                rc = qore_socket_get_port_direct(sock);
                break;
            case Action::SetMaxChunkedBodySize: {
                qore_socket_private* priv = qore_socket_private::get(*sock);
                priv->max_chunked_body_size.store(value, std::memory_order_relaxed);
                rc = 0;
                break;
            }
            case Action::SetSslVerifyMode: {
                qore_socket_private* priv = qore_socket_private::get(*sock);
                priv->setSslVerifyMode(static_cast<int>(value));
                rc = 0;
                break;
            }
            case Action::GetSslVerifyMode: {
                qore_socket_private* priv = qore_socket_private::get(*sock);
                rc = priv->ssl_verify_mode;
                break;
            }
            case Action::SetAcceptAllCertificates: {
                qore_socket_private* priv = qore_socket_private::get(*sock);
                priv->acceptAllCertificates(value != 0);
                rc = 0;
                break;
            }
            case Action::GetAcceptAllCertificates: {
                qore_socket_private* priv = qore_socket_private::get(*sock);
                rc = priv->ssl_accept_all_certs ? 1 : 0;
                break;
            }
            case Action::CaptureRemoteCertificates: {
                qore_socket_private* priv = qore_socket_private::get(*sock);
                rc = priv->ssl_capture_remote_cert ? 1 : 0;
                priv->ssl_capture_remote_cert = value != 0;
                break;
            }
            case Action::SetHttp2MaxRequestBodySize: {
                qore_socket_private* priv = qore_socket_private::get(*sock);
                priv->max_http2_body_size.store(value, std::memory_order_relaxed);
                AutoLocker al(priv->h2_session_lock);
                if (priv->h2_session) {
                    priv->h2_session->setMaxRequestBodySize(value);
                }
                rc = 0;
                break;
            }
        }

        done = true;
        return nullptr;
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return rc;
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        if (done) {
            return "done";
        }
        if (action == Action::Listen) {
            return "listening";
        }
        if (action == Action::Shutdown) {
            return "shutting-down";
        }
        if (isConfigAction()) {
            return "configuring";
        }
        if (resolver) {
            return "resolving";
        }
        return "binding";
    }

    DLLLOCAL bool isConfigAction() const {
        return action == Action::SetNoDelay
            || action == Action::GetNoDelay
            || action == Action::SetUserTimeout
            || action == Action::GetUserTimeout
            || action == Action::SetSendTimeout
            || action == Action::SetRecvTimeout
            || action == Action::GetSendTimeout
            || action == Action::GetRecvTimeout
            || action == Action::GetPort
            || action == Action::SetMaxChunkedBodySize
            || action == Action::SetSslVerifyMode
            || action == Action::GetSslVerifyMode
            || action == Action::SetAcceptAllCertificates
            || action == Action::GetAcceptAllCertificates
            || action == Action::CaptureRemoteCertificates
            || action == Action::SetHttp2MaxRequestBodySize;
    }

private:
    DLLLOCAL QoreHashNode* continueBindInet(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* getResolverPollInfo(ExceptionSink* xsink) const;

    DLLLOCAL static Action getAction(ConfigAction config_action) {
        switch (config_action) {
            case ConfigAction::SetNoDelay:
                return Action::SetNoDelay;
            case ConfigAction::GetNoDelay:
                return Action::GetNoDelay;
            case ConfigAction::SetUserTimeout:
                return Action::SetUserTimeout;
            case ConfigAction::GetUserTimeout:
                return Action::GetUserTimeout;
            case ConfigAction::SetSendTimeout:
                return Action::SetSendTimeout;
            case ConfigAction::SetRecvTimeout:
                return Action::SetRecvTimeout;
            case ConfigAction::GetSendTimeout:
                return Action::GetSendTimeout;
            case ConfigAction::GetRecvTimeout:
                return Action::GetRecvTimeout;
            case ConfigAction::GetPort:
                return Action::GetPort;
            case ConfigAction::SetMaxChunkedBodySize:
                return Action::SetMaxChunkedBodySize;
            case ConfigAction::SetSslVerifyMode:
                return Action::SetSslVerifyMode;
            case ConfigAction::GetSslVerifyMode:
                return Action::GetSslVerifyMode;
            case ConfigAction::SetAcceptAllCertificates:
                return Action::SetAcceptAllCertificates;
            case ConfigAction::GetAcceptAllCertificates:
                return Action::GetAcceptAllCertificates;
            case ConfigAction::CaptureRemoteCertificates:
                return Action::CaptureRemoteCertificates;
            case ConfigAction::SetHttp2MaxRequestBodySize:
                return Action::SetHttp2MaxRequestBodySize;
        }
        assert(false);
        return Action::GetNoDelay;
    }

    DLLLOCAL void copyAddress(const struct sockaddr* source, int size, ExceptionSink* xsink) {
        if (!source || size <= 0) {
            xsink->raiseException("SOCKET-BIND-ERROR", "invalid socket address for Socket::bind()");
            return;
        }
        if (static_cast<size_t>(size) > sizeof(addr)) {
            xsink->raiseException("SOCKET-BIND-ERROR",
                "socket address size %d exceeds maximum supported size %zu", size, sizeof(addr));
            return;
        }
        memcpy(&addr, source, size);
    }

    QoreSocket* sock;
    Action action;
    std::string name;
    std::string service;
    bool has_name = false;
    bool has_service = false;
    bool reuseaddr = false;
    int port = 0;
    int family = 0;
    int socktype = 0;
    int protocol = 0;
    int backlog = 0;
    int64 value = 0;
    int addr_size = 0;
    sockaddr_storage addr = {};
    int rc = -1;
    std::unique_ptr<QoreCaresAddrInfoResolver> resolver;
    std::vector<SocketResolvedAddrInfo> bind_inet_addrs;
    bool bind_inet_resolved = false;
    bool done = false;
};

class QoreSocketControllerAcceptReplacePollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketControllerAcceptReplacePollOperation(QoreSocket* sock, SocketSource* source)
            : sock(sock), source(source) {
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink*) override {
        poll_state.reset();
        accepted_socket.reset();
        done = true;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (done) {
            return nullptr;
        }

        if (!accepted_socket) {
            if (!poll_state) {
                poll_state.reset(new SocketAcceptPollState(xsink, qore_socket_private::get(*sock), source));
                if (*xsink || !poll_state) {
                    done = true;
                    return nullptr;
                }
            }

            int rc = poll_state->continuePoll(xsink);
            if (*xsink || rc < 0) {
                poll_state.reset();
                done = true;
                return nullptr;
            }
            if (rc) {
                return getSocketPollInfoHash(xsink, rc);
            }

            assert(dynamic_cast<SocketAcceptPollState*>(poll_state.get()));
            int accepted_fd = reinterpret_cast<SocketAcceptPollState*>(poll_state.get())->getDescriptor();
            poll_state.reset();
            accepted_socket.reset(sock->createAcceptedSocket(accepted_fd));
        }

        qore_socket_private* priv = qore_socket_private::get(*sock);
        qore_socket_private* accepted_priv = qore_socket_private::get(*accepted_socket);
        qore_socket_close_private_from_controller(priv);
        assert(priv->sock == QORE_INVALID_SOCKET);
        priv->sock = accepted_priv->sock;
        accepted_priv->sock = QORE_INVALID_SOCKET;
        accepted_socket.reset();
        priv->resetCloseInterrupt();
        rc = 0;
        done = true;
        return nullptr;
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return rc;
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        if (done) {
            return "done";
        }
        return accepted_socket ? "accept-replace" : "accepting";
    }

private:
    QoreSocket* sock;
    SocketSource* source = nullptr;
    std::unique_ptr<AbstractPollState> poll_state;
    std::unique_ptr<QoreSocket> accepted_socket;
    int rc = -1;
    bool done = false;
};

class QoreSocketPollListPollable : public AbstractPollableIoObjectBase {
public:
    DLLLOCAL explicit QoreSocketPollListPollable(int fd) : fd(fd) {
    }

    DLLLOCAL virtual int getPollableDescriptor() const override {
        return fd;
    }

    DLLLOCAL virtual void closeIo(ExceptionSink*) override {
        // Socket::poll() observes readiness only; it never owns the fd.
    }

private:
    int fd;
};

class QoreSocketPollListReadinessPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketPollListReadinessPollOperation(QoreObject* sock_obj, int events,
            std::shared_ptr<std::atomic<bool>> armed)
            : sock_obj(sock_obj), events(events), armed(std::move(armed)) {
    }

    DLLLOCAL virtual bool goalReached() const override {
        return ready;
    }

    DLLLOCAL virtual void abort(ExceptionSink*) override {
        waiting = false;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (ready) {
            return nullptr;
        }
        if (isPollableClosed()) {
            ready = true;
            output_events = SOCK_POLLERR;
            return nullptr;
        }
        if (!waiting) {
            waiting = true;
            return getPollInfo(xsink);
        }
        if (!armed->load(std::memory_order_acquire)) {
            return getPollInfo(xsink);
        }
        ready = true;
        output_events = ((ready_events & SOCK_POLLERR) || isPollableClosed())
            ? SOCK_POLLERR
            : (ready_events ? (ready_events & events) : events);
        return nullptr;
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return ready ? output_events : 0;
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        return ready ? "ready" : "waiting";
    }

    DLLLOCAL virtual void setReadyEvents(int events) override {
        ready_events = events;
    }

private:
    QoreObject* sock_obj;
    int events;
    std::shared_ptr<std::atomic<bool>> armed;
    int ready_events = 0;
    int output_events = 0;
    bool waiting = false;
    bool ready = false;

    DLLLOCAL QoreHashNode* getPollInfo(ExceptionSink* xsink) const {
        ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollInfo, xsink), xsink);
        info->setKeyValue("events", events, xsink);
        info->setKeyValue("socket", sock_obj->objectRefSelf(), xsink);
        if (*xsink) {
            return nullptr;
        }
        return info.release();
    }

    DLLLOCAL bool isPollableClosed() const {
        if (!sock_obj->isCurrent()) {
            return true;
        }
        ExceptionSink xsink;
        AbstractPollableIoObjectBase* io = static_cast<AbstractPollableIoObjectBase*>(
            sock_obj->getReferencedPrivateData(CID_ABSTRACTPOLLABLEIOOBJECTBASE, &xsink));
        if (xsink) {
            xsink.clear();
            return true;
        }
        if (!io) {
            return true;
        }
        ReferenceHolder<AbstractPollableIoObjectBase> holder(io, &xsink);
        return io->getPollableDescriptor() < 0;
    }
};

class QoreSocketControllerDeferredPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketControllerDeferredPollOperation(QoreSocket* sock, const QoreEncoding* enc,
            bool to_string, bool capture_output, const char* active_state, const char* done_state)
            : sock(sock), enc(enc), to_string(to_string), capture_output(capture_output),
            active_state(active_state), done_state(done_state) {
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink*) override {
        bool close_socket = poll_state && abortNeedsClose(*poll_state);
        poll_state.reset();
        if (close_socket) {
            qore_socket_close_from_controller(sock);
        }
        done = true;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (done) {
            return nullptr;
        }

        if (!poll_state) {
            poll_state.reset(createPollState(xsink));
            if (*xsink || !poll_state) {
                return nullptr;
            }
        }

        int rc = poll_state->continuePoll(xsink);
        if (!rc && capture_output) {
            SimpleRefHolder<BinaryNode> bin(poll_state->takeOutput().get<BinaryNode>());
            if (to_string) {
                size_t len = bin->size();
                char* buf = reinterpret_cast<char*>(bin->giveBuffer());
                char* nbuf = reinterpret_cast<char*>(q_realloc(buf, len + 1));
                if (!nbuf) {
                    xsink->outOfMemory();
                } else {
                    nbuf[len] = '\0';
                    data = new QoreStringNode(nbuf, len, len + 1, enc);
                }
            } else {
                data = bin.release();
            }
        }
        if (*xsink || rc <= 0) {
            poll_state.reset();
            if (!*xsink) {
                done = true;
            }
            return nullptr;
        }
        return getSocketPollInfoHash(xsink, rc);
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return data ? data->refSelf() : QoreValue();
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        return done ? done_state : active_state;
    }

protected:
    DLLLOCAL virtual AbstractPollState* createPollState(ExceptionSink* xsink) = 0;
    DLLLOCAL virtual bool abortNeedsClose(const AbstractPollState&) const {
        return false;
    }

    QoreSocket* sock;

private:
    std::unique_ptr<AbstractPollState> poll_state;
    const QoreEncoding* enc;
    bool to_string;
    bool capture_output;
    bool done = false;
    const char* active_state;
    const char* done_state;
    SimpleRefHolder<SimpleValueQoreNode> data;
};

class QoreSocketControllerSendBytesPollOperation : public QoreSocketControllerDeferredPollOperation {
public:
    DLLLOCAL QoreSocketControllerSendBytesPollOperation(QoreSocket* sock, const void* data, size_t size,
            const char* http1_method = nullptr)
            : QoreSocketControllerDeferredPollOperation(sock, sock->getEncoding(), false, false, "sending", "sent"),
            data(new BinaryNode), http1_method(http1_method ? http1_method : "") {
        this->data->append(data, size);
    }

protected:
    DLLLOCAL virtual AbstractPollState* createPollState(ExceptionSink* xsink) override {
        if (!http1_method.empty()) {
            qore_socket_private* priv = qore_socket_private::get(*sock);
            if (priv->h2_session) {
                xsink->raiseException("HTTP2-ERROR",
                    "HTTP/1 message attempted on HTTP/2 connection (Socket::%s)", http1_method.c_str());
                return nullptr;
            }
        }
        return sock->startSend(xsink, reinterpret_cast<const char*>(data->getPtr()), data->size());
    }

    DLLLOCAL virtual bool abortNeedsClose(const AbstractPollState& poll_state) const override {
        const SocketSendPollState& send_state = static_cast<const SocketSendPollState&>(poll_state);
        return send_state.getBytesSent() > 0;
    }

private:
    SimpleRefHolder<BinaryNode> data;
    std::string http1_method;
};

class QoreSocketControllerHttp1AllowedPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketControllerHttp1AllowedPollOperation(QoreSocket* sock, const char* mname)
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

        qore_socket_private* priv = qore_socket_private::get(*sock);
        if (priv->h2_session) {
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
    QoreSocket* sock;
    std::string mname;
    bool done = false;
};

class QoreSocketControllerHttp2ServerStreamPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketControllerHttp2ServerStreamPollOperation(QoreSocket* sock, int32_t stream_id,
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

        qore_socket_private* priv = qore_socket_private::get(*sock);
        if (!priv->h2_session) {
            output = -1;
        } else if (priv->h2_session->isServer() && stream_id > 0) {
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
    QoreSocket* sock;
    int32_t stream_id;
    std::string mname;
    int32_t output = -1;
    bool done = false;
};

class QoreSocketControllerRecvPollOperation : public QoreSocketControllerDeferredPollOperation {
public:
    enum class Action {
        Recv,
        RecvPacket,
        RecvSome,
        RecvUntilBytes,
    };

    DLLLOCAL QoreSocketControllerRecvPollOperation(QoreSocket* sock, Action action, size_t size,
            bool to_string, const char* active_state, const char* done_state)
            : QoreSocketControllerDeferredPollOperation(sock, sock->getEncoding(), to_string, true,
                active_state, done_state), action(action), size(size) {
    }

    DLLLOCAL QoreSocketControllerRecvPollOperation(QoreSocket* sock, const char* pattern, size_t pattern_size,
            bool to_string, const char* active_state, const char* done_state)
            : QoreSocketControllerDeferredPollOperation(sock, sock->getEncoding(), to_string, true,
                active_state, done_state), action(Action::RecvUntilBytes), pattern(pattern, pattern_size) {
    }

protected:
    DLLLOCAL virtual AbstractPollState* createPollState(ExceptionSink* xsink) override {
        switch (action) {
            case Action::Recv:
                return sock->startRecv(xsink, size);
            case Action::RecvPacket:
                return sock->startRecvPacket(xsink);
            case Action::RecvSome:
                return sock->startRecvSome(xsink, size);
            case Action::RecvUntilBytes:
                return sock->startRecvUntilBytes(xsink, pattern.c_str(), pattern.size());
            default:
                assert(false);
        }
        return nullptr;
    }

    DLLLOCAL virtual bool abortNeedsClose(const AbstractPollState& poll_state) const override {
        switch (action) {
            case Action::Recv:
                return static_cast<const SocketRecvPollState&>(poll_state).getBytesReceived() > 0;
            case Action::RecvPacket:
                return static_cast<const SocketRecvPacketPollState&>(poll_state).getBytesReceived() > 0;
            case Action::RecvSome:
                return static_cast<const SocketRecvSomePollState&>(poll_state).getBytesReceived() > 0;
            case Action::RecvUntilBytes:
                return static_cast<const SocketRecvUntilBytesPollState&>(poll_state).getBytesReceived() > 0;
            default:
                assert(false);
        }
        return false;
    }

private:
    Action action;
    size_t size = 0;
    std::string pattern;
};

class QoreSocketControllerReadServerSentEventPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketControllerReadServerSentEventPollOperation(QoreSocket* sock)
            : sock(sock), event_data(QCS_UTF8), compressed_data(QCS_DEFAULT), out(nullptr) {
    }

    DLLLOCAL QoreSocketControllerReadServerSentEventPollOperation(QoreSocket* sock,
            const QoreStringNode* content_encoding, ExceptionSink* xsink)
            : sock(sock), event_data(QCS_UTF8), compressed_data(QCS_DEFAULT), out(xsink) {
        initTransform(content_encoding, xsink);
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink*) override {
        bool close_socket = bytes_consumed;
        if (!close_socket && poll_state) {
            if (transform) {
                assert(dynamic_cast<SocketRecvSomePollState*>(poll_state.get()));
                close_socket = reinterpret_cast<SocketRecvSomePollState*>(poll_state.get())->getBytesReceived() > 0;
            } else {
                assert(dynamic_cast<SocketRecvPollState*>(poll_state.get()));
                close_socket = reinterpret_cast<SocketRecvPollState*>(poll_state.get())->getBytesReceived() > 0;
            }
        }
        poll_state.reset();
        if (close_socket) {
            qore_socket_close_from_controller(sock);
        }
        done = true;
    }

    DLLLOCAL void initTransform(const QoreStringNode* content_encoding, ExceptionSink* xsink) {
        if (!content_encoding) {
            return;
        }

        transform = CompressionTransforms::getDecompressor(content_encoding, xsink);
        if (*xsink) {
            return;
        }

        transform_buf_size = transform->outputBufferSize();
        if (!transform_buf_size) {
            return;
        }
        transform_buf.reset(new (std::nothrow) char[transform_buf_size]);
        if (!transform_buf) {
            xsink->outOfMemory();
        }
    }

    DLLLOCAL bool processSseChar(ExceptionSink* xsink, qore_socket_private* priv, char c) {
        if (!qore_socket_exec_process_sse_char(priv, event_data, eol_count, c)) {
            return false;
        }

        out = QoreSocket::parseServerSentEvent(xsink, event_data);
        if (!*xsink) {
            done = true;
        }
        return true;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (done) {
            return nullptr;
        }

        qore_socket_private* priv = qore_socket_private::get(*sock);
        while (true) {
            if (transform && transform_pos < transform_len) {
                char c = transform_buf[transform_pos++];
                if (processSseChar(xsink, priv, c)) {
                    return nullptr;
                }
                continue;
            }

            if (!poll_state) {
                poll_state.reset(transform
                    ? sock->startRecvSome(xsink, DEFAULT_SOCKET_BUFSIZE)
                    : sock->startRecv(xsink, 1));
                if (*xsink || !poll_state) {
                    return nullptr;
                }
            }

            int rc = poll_state->continuePoll(xsink);
            if (*xsink) {
                poll_state.reset();
                return nullptr;
            }
            if (rc) {
                return getSocketPollInfoHash(xsink, rc);
            }

            SimpleRefHolder<BinaryNode> data(poll_state->takeOutput().get<BinaryNode>());
            poll_state.reset();
            if (!data || !data->size()) {
                se_closed("Socket", "readServerSentEvent", xsink);
                return nullptr;
            }

            bytes_consumed = true;
            if (transform) {
                compressed_data.concat(static_cast<const char*>(data->getPtr()), data->size());
                transform_pos = 0;
                std::pair<int64, int64> result = transform->apply(compressed_data.c_str(), compressed_data.size(),
                    transform_buf.get(), transform_buf_size, xsink);
                if (*xsink) {
                    return nullptr;
                }
                if (result.first) {
                    compressed_data.removeBytes(result.first);
                }
                transform_len = static_cast<size_t>(result.second);
                if (!transform_len) {
                    continue;
                }
                continue;
            }

            char c = *static_cast<const char*>(data->getPtr());
            if (processSseChar(xsink, priv, c)) {
                return nullptr;
            }
        }
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return out.release();
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        return done ? "received" : "reading-sse";
    }

private:
    QoreSocket* sock;
    std::unique_ptr<AbstractPollState> poll_state;
    QoreString event_data;
    QoreString compressed_data;
    mutable ReferenceHolder<QoreHashNode> out;
    SimpleRefHolder<Transform> transform;
    std::unique_ptr<char[]> transform_buf;
    size_t transform_buf_size = 0;
    size_t transform_len = 0;
    size_t transform_pos = 0;
    int eol_count = 0;
    bool bytes_consumed = false;
    bool done = false;
};

class QoreSocketControllerConnectPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketControllerConnectPollOperation(QoreSocket* sock, const char* target, bool ssl = false,
            QoreSSLCertificate* cert = nullptr, QoreSSLPrivateKey* pkey = nullptr)
            : sock(sock), target(target), ssl(ssl), cert(cert ? cert->certRefSelf() : nullptr),
            pkey(pkey ? pkey->pkRefSelf() : nullptr),
            sandbox_manager(qore_socket_ref_current_sandbox_manager()) {
    }

    DLLLOCAL QoreSocketControllerConnectPollOperation(QoreSocket* sock, const char* host, const char* service,
            int family, int socktype, int protocol, bool ssl = false, QoreSSLCertificate* cert = nullptr,
            QoreSSLPrivateKey* pkey = nullptr)
            : sock(sock), target(host), service(service), connect_target(ConnectTarget::Inet), family(family),
            socktype(socktype), protocol(protocol), ssl(ssl), cert(cert ? cert->certRefSelf() : nullptr),
            pkey(pkey ? pkey->pkRefSelf() : nullptr),
            sandbox_manager(qore_socket_ref_current_sandbox_manager()) {
    }

    DLLLOCAL QoreSocketControllerConnectPollOperation(QoreSocket* sock, const char* path, int socktype,
            int protocol, bool ssl = false, QoreSSLCertificate* cert = nullptr, QoreSSLPrivateKey* pkey = nullptr)
            : sock(sock), target(path), connect_target(ConnectTarget::Unix), socktype(socktype),
            protocol(protocol), ssl(ssl), cert(cert ? cert->certRefSelf() : nullptr),
            pkey(pkey ? pkey->pkRefSelf() : nullptr),
            sandbox_manager(qore_socket_ref_current_sandbox_manager()) {
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink*) override {
        bool close_socket = static_cast<bool>(poll_state);
        poll_state.reset();
        if (close_socket) {
            qore_socket_close_from_controller(sock);
        }
        done = true;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (done) {
            return nullptr;
        }

        while (true) {
            if (!poll_state) {
                poll_state.reset(phase == Phase::Connect ? startConnect(xsink) : startSslConnect(xsink));
                if (*xsink) {
                    return nullptr;
                }
                if (!poll_state) {
                    if (phase == Phase::Connect && ssl) {
                        phase = Phase::Ssl;
                        continue;
                    }
                    done = true;
                    return nullptr;
                }
            }

            int rc = poll_state->continuePoll(xsink);
            if (*xsink || rc < 0) {
                poll_state.reset();
                return nullptr;
            }
            if (!rc) {
                poll_state.reset();
                if (phase == Phase::Connect && ssl) {
                    phase = Phase::Ssl;
                    continue;
                }
                done = true;
                return nullptr;
            }

            if (phase == Phase::Connect) {
                auto* he_state = dynamic_cast<SocketConnectInetHappyEyeballsPollState*>(poll_state.get());
                if (he_state && (he_state->getState() == HEBS_RESOLVING
                        || (he_state->isRacing() && he_state->getState() == HEBS_RACING))) {
                    std::vector<std::pair<int, int>> extra_fds;
                    he_state->getExtraFds(extra_fds);
                    bumpFdGeneration();
                    QoreHashNode* rv = getSocketPollInfoHash(xsink, he_state->getPrimaryPollEvents(rc),
                        extra_fds);
                    if (rv) {
                        int poll_timeout_ms = he_state->getPollTimeoutMs();
                        if (poll_timeout_ms >= 0) {
                            rv->setKeyValue("poll_timeout_ms", poll_timeout_ms, xsink);
                        }
                    }
                    return rv;
                }
            }

            return getSocketPollInfoHash(xsink, rc);
        }
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        if (done) {
            return ssl ? "ssl-connected" : "connected";
        }
        return phase == Phase::Ssl ? "connecting-ssl" : "connecting";
    }

private:
    enum class Phase {
        Connect,
        Ssl,
    };

    enum class ConnectTarget {
        Auto,
        Inet,
        Unix,
    };

    DLLLOCAL AbstractPollState* startConnect(ExceptionSink* xsink) {
        switch (connect_target) {
            case ConnectTarget::Auto:
                if (const char* p = strrchr(target.c_str(), ':')) {
                    QoreString host(target.c_str(), p - target.c_str());
                    QoreString service(p + 1);
                    if (host.strlen() > 2 && host[0] == '[' && host[host.strlen() - 1] == ']') {
                        host.terminate(host.strlen() - 1);
                        return new SocketConnectInetHappyEyeballsPollState(xsink, qore_socket_private::get(*sock),
                            host.c_str() + 1, service.c_str(), AF_INET6, SOCK_STREAM, 0, *sandbox_manager);
                    }
                    return new SocketConnectInetHappyEyeballsPollState(xsink, qore_socket_private::get(*sock),
                        host.c_str(), service.c_str(), AF_UNSPEC, SOCK_STREAM, 0, *sandbox_manager);
                }
#ifndef _Q_WINDOWS
                return new SocketConnectUnixPollState(xsink, qore_socket_private::get(*sock), target.c_str(),
                    SOCK_STREAM, 0, *sandbox_manager);
#else
                missing_function_error("Socket::startConnect(<UNIX socket file>)", "UNIX_FILEMGT", xsink);
                return nullptr;
#endif
            case ConnectTarget::Inet:
                return new SocketConnectInetHappyEyeballsPollState(xsink, qore_socket_private::get(*sock),
                    target.c_str(), service.c_str(), family, socktype, protocol, *sandbox_manager);
            case ConnectTarget::Unix:
#ifndef _Q_WINDOWS
                return new SocketConnectUnixPollState(xsink, qore_socket_private::get(*sock), target.c_str(),
                    socktype, protocol, *sandbox_manager);
#else
                missing_function_error("Socket::startConnect(<UNIX socket file>)", "UNIX_FILEMGT", xsink);
                return nullptr;
#endif
            default:
                assert(false);
        }
        return nullptr;
    }

    DLLLOCAL AbstractPollState* startSslConnect(ExceptionSink* xsink) {
        return sock->startSslConnect(xsink, *cert, *pkey);
    }

    QoreSocket* sock;
    std::unique_ptr<AbstractPollState> poll_state;
    std::string target;
    std::string service;
    ConnectTarget connect_target = ConnectTarget::Auto;
    int family = Q_AF_UNSPEC;
    int socktype = Q_SOCK_STREAM;
    int protocol = 0;
    Phase phase = Phase::Connect;
    bool ssl = false;
    SimpleRefHolder<QoreSSLCertificate> cert;
    SimpleRefHolder<QoreSSLPrivateKey> pkey;
    SimpleRefHolder<QoreSandboxManager> sandbox_manager;
    bool done = false;
};

class QoreSocketControllerSslUpgradePollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketControllerSslUpgradePollOperation(QoreSocket* sock, bool server,
            QoreSSLCertificate* cert, QoreSSLPrivateKey* pkey)
            : sock(sock), server(server), cert(cert ? cert->certRefSelf() : nullptr),
            pkey(pkey ? pkey->pkRefSelf() : nullptr) {
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink*) override {
        bool close_socket = static_cast<bool>(poll_state);
        poll_state.reset();
        if (close_socket) {
            qore_socket_close_from_controller(sock);
        }
        done = true;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (done) {
            return nullptr;
        }

        if (!poll_state) {
            qore_socket_private* priv = qore_socket_private::get(*sock);
            if (priv->sock == QORE_INVALID_SOCKET) {
                se_not_open("Socket", server ? "upgradeServerToSSL" : "upgradeClientToSSL", xsink);
                return nullptr;
            }
            if (priv->ssl) {
                done = true;
                return nullptr;
            }

            poll_state.reset(server
                ? sock->startSslAccept(xsink, *cert, *pkey)
                : sock->startSslConnect(xsink, *cert, *pkey));
            if (*xsink || !poll_state) {
                return nullptr;
            }
        }

        int rc = poll_state->continuePoll(xsink);
        if (*xsink || rc <= 0) {
            poll_state.reset();
            if (!*xsink) {
                done = true;
            }
            return nullptr;
        }
        return getSocketPollInfoHash(xsink, rc);
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        if (done) {
            return "ssl-upgraded";
        }
        return server ? "upgrading-server-ssl" : "upgrading-client-ssl";
    }

private:
    QoreSocket* sock;
    bool server;
    SimpleRefHolder<QoreSSLCertificate> cert;
    SimpleRefHolder<QoreSSLPrivateKey> pkey;
    std::unique_ptr<AbstractPollState> poll_state;
    bool done = false;
};

class QoreSocketControllerSslShutdownPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketControllerSslShutdownPollOperation(QoreSocket* sock) : sock(sock) {
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink*) override {
        poll_state.reset();
        done = true;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (done) {
            return nullptr;
        }

        if (!poll_state) {
            qore_socket_private* priv = qore_socket_private::get(*sock);
            if (priv->sock == QORE_INVALID_SOCKET || !priv->ssl) {
                done = true;
                return nullptr;
            }
            poll_state.reset(new SocketShutdownSslPollState(xsink, priv));
            if (*xsink || !poll_state) {
                return nullptr;
            }
        }

        int rc = poll_state->continuePoll(xsink);
        if (*xsink || rc <= 0) {
            poll_state.reset();
            if (!*xsink) {
                done = true;
            }
            return nullptr;
        }
        return getSocketPollInfoHash(xsink, rc);
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        return done ? "ssl-shutdown" : "shutting-down-ssl";
    }

private:
    QoreSocket* sock;
    std::unique_ptr<AbstractPollState> poll_state;
    bool done = false;
};

class QoreSocketControllerSendInputStreamPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketControllerSendInputStreamPollOperation(ExceptionSink* xsink, QoreSocket* sock,
            InputStream* input_stream, int64 size, int timeout_ms)
            : sock(sock), input_stream(input_stream), size(size), timeout_ms(timeout_ms) {
        if (!input_stream->isIoThreadSafe()) {
            xsink->raiseException("SOCKET-SEND-ERROR", "InputStream is not I/O thread safe");
            return;
        }

        stream_fd = qore_input_stream_pollable_descriptor(input_stream, "SOCKET-SEND-ERROR", xsink);
        if (*xsink) {
            return;
        }
        is_pollable = stream_fd >= 0;
    }

    DLLLOCAL void deref(ExceptionSink* xsink) override {
        if (ROdereference()) {
            poll_state.reset();
            if (input_stream && !need_reassign) {
                input_stream->unassignThread(xsink);
            }
            input_stream = nullptr;
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return phase == Phase::Done;
    }

    DLLLOCAL virtual void abort(ExceptionSink* xsink) override {
        if (input_stream && !need_reassign) {
            input_stream->unassignThread(xsink);
        }
        input_stream = nullptr;
        current_chunk = nullptr;
        poll_state.reset();
        if (socket_data_sent || bytes_sent > 0) {
            qore_socket_close_from_controller(sock);
        }
        phase = Phase::Error;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (need_reassign) {
            need_reassign = false;
            if (input_stream) {
                input_stream->reassignThread(xsink);
                if (*xsink) {
                    phase = Phase::Error;
                    return nullptr;
                }
            }
        }

        qore_socket_private* priv = qore_socket_private::get(*sock);
        if (!priv->isOpen()) {
            se_not_open("Socket", "send", xsink);
            phase = Phase::Error;
            return nullptr;
        }
        if (checkTimeout(xsink)) {
            return nullptr;
        }

        unsigned loop = 0;
        while (true) {
            switch (phase) {
                case Phase::ReadChunk: {
                    if (size >= 0 && bytes_sent >= size) {
                        complete(xsink);
                        return nullptr;
                    }

                    int64 chunk_size = getNextChunkSize();
                    if (chunk_size <= 0) {
                        complete(xsink);
                        return nullptr;
                    }

                    if (is_pollable) {
                        assert(stream_fd >= 0);
                        int poll_rv = qore_socket_poll_fd_now(stream_fd, POLLIN, "SOCKET-SEND-ERROR", "stream",
                            xsink);
                        if (poll_rv < 0) {
                            phase = Phase::Error;
                            return nullptr;
                        }
                        if (!poll_rv) {
                            return getPollInfo(xsink, SOCK_POLLIN, true);
                        }

                        SimpleRefHolder<BinaryNode> chunk(new BinaryNode);
                        chunk->preallocate(chunk_size);
                        int64 count = input_stream->readNonBlock(
                            const_cast<void*>(chunk->getPtr()), chunk_size, xsink);
                        if (*xsink) {
                            phase = Phase::Error;
                            return nullptr;
                        }
                        if (count <= 0) {
                            if (size >= 0) {
                                xsink->raiseException("SOCKET-SEND-ERROR", "Unexpected end of stream");
                                phase = Phase::Error;
                                return nullptr;
                            }
                            complete(xsink);
                            return nullptr;
                        }
                        chunk->setSize(count);
                        current_chunk = chunk.release();
                        clearTimeout();
                    } else {
                        current_chunk = input_stream->readHelper(chunk_size, xsink);
                        if (*xsink) {
                            phase = Phase::Error;
                            return nullptr;
                        }
                        if (!current_chunk) {
                            if (size >= 0) {
                                xsink->raiseException("SOCKET-SEND-ERROR", "Unexpected end of stream");
                                phase = Phase::Error;
                                return nullptr;
                            }
                            complete(xsink);
                            return nullptr;
                        }
                        clearTimeout();
                    }

                    phase = Phase::SendChunk;
                    continue;
                }

                case Phase::SendChunk: {
                    if (!poll_state) {
                        poll_state.reset(sock->startSend(xsink,
                            reinterpret_cast<const char*>(current_chunk->getPtr()), current_chunk->size()));
                        if (*xsink || !poll_state) {
                            phase = Phase::Error;
                            return nullptr;
                        }
                    }

                    SocketSendPollState* send_state = static_cast<SocketSendPollState*>(poll_state.get());
                    if (send_state->getBytesSent()) {
                        socket_data_sent = true;
                    }
                    size_t sent_before = send_state->getBytesSent();
                    int rc = poll_state->continuePoll(xsink);
                    if (*xsink) {
                        phase = Phase::Error;
                        poll_state.reset();
                        return nullptr;
                    }
                    if (send_state->getBytesSent() > sent_before) {
                        socket_data_sent = true;
                        clearTimeout();
                    }
                    if (rc) {
                        return getPollInfo(xsink, rc);
                    }

                    poll_state.reset();
                    bytes_sent += current_chunk->size();
                    qore_socket_private::get(*sock)->do_data_event(
                        QORE_EVENT_SOCKET_DATA_SENT, QORE_SOURCE_SOCKET, **current_chunk);
                    current_chunk = nullptr;
                    if (++loop >= max_nonblock_ops && (size < 0 || bytes_sent < size)) {
                        return getPollInfo(xsink, SOCK_POLLOUT);
                    }
                    phase = Phase::ReadChunk;
                    continue;
                }

                case Phase::Done:
                case Phase::Error:
                    return nullptr;
            }
        }
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        switch (phase) {
            case Phase::ReadChunk: return "reading-chunk";
            case Phase::SendChunk: return "sending-chunk";
            case Phase::Done: return "done";
            case Phase::Error: return "error";
            default: return "unknown";
        }
    }

private:
    enum class Phase { ReadChunk, SendChunk, Done, Error };

    DLLLOCAL QoreHashNode* getPollInfo(ExceptionSink* xsink, int events, bool stream_wait = false) {
        int64 poll_timeout_ms = -1;
        if (timeout_ms >= 0) {
            int us;
            int64 now_s = q_epoch_us(us);
            int64 now_ms = now_s * 1000 + us / 1000;
            if (!wait_deadline_ms) {
                wait_deadline_ms = now_ms + timeout_ms;
            }
            poll_timeout_ms = wait_deadline_ms - now_ms;
            if (poll_timeout_ms < 0) {
                poll_timeout_ms = 0;
            }
        }

        QoreHashNode* raw_info = nullptr;
        if (stream_wait && is_pollable && stream_fd >= 0) {
            std::vector<std::pair<int, int>> extra_fds{{stream_fd, SOCK_POLLIN}};
            raw_info = getSocketPollInfoHash(xsink, 0, extra_fds);
        } else {
            raw_info = getSocketPollInfoHash(xsink, events);
        }
        if (!raw_info) {
            return nullptr;
        }
        ReferenceHolder<QoreHashNode> info(raw_info, xsink);
        if (poll_timeout_ms >= 0) {
            info->setKeyValue("poll_timeout_ms", poll_timeout_ms, xsink);
        }
        return info.release();
    }

    DLLLOCAL int64 getNextChunkSize() const {
        if (size < 0) {
            return DEFAULT_SOCKET_BUFSIZE;
        }
        return QORE_MIN(size - bytes_sent, (int64)DEFAULT_SOCKET_BUFSIZE);
    }

    DLLLOCAL void complete(ExceptionSink* xsink) {
        if (input_stream && !need_reassign) {
            input_stream->unassignThread(xsink);
            if (*xsink) {
                phase = Phase::Error;
                return;
            }
        }
        input_stream = nullptr;
        phase = Phase::Done;
    }

    DLLLOCAL bool checkTimeout(ExceptionSink* xsink) {
        if (wait_deadline_ms <= 0) {
            return false;
        }
        int us;
        int64 now_s = q_epoch_us(us);
        int64 now_ms = now_s * 1000 + us / 1000;
        if (now_ms < wait_deadline_ms) {
            return false;
        }
        xsink->raiseException("SOCKET-TIMEOUT", "socket operation timed out");
        phase = Phase::Error;
        return true;
    }

    DLLLOCAL void clearTimeout() {
        wait_deadline_ms = 0;
    }

    QoreSocket* sock;
    std::unique_ptr<AbstractPollState> poll_state;
    SimpleRefHolder<InputStream> input_stream;
    int64 size = -1;
    int64 bytes_sent = 0;
    int timeout_ms = -1;
    int64 wait_deadline_ms = 0;
    int stream_fd = -1;
    bool is_pollable = true;
    bool need_reassign = true;
    bool socket_data_sent = false;
    Phase phase = Phase::ReadChunk;
    SimpleRefHolder<BinaryNode> current_chunk;
};

class QoreSocketControllerRecvOutputStreamPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketControllerRecvOutputStreamPollOperation(ExceptionSink* xsink, QoreSocket* sock,
            OutputStream* output_stream, int64 size, int timeout_ms)
            : sock(sock), output_stream(output_stream), size(size), timeout_ms(timeout_ms) {
        if (!output_stream->isIoThreadSafe()) {
            xsink->raiseException("SOCKET-RECV-ERROR", "OutputStream is not I/O thread safe");
            return;
        }

        output_fd = qore_output_stream_pollable_descriptor(output_stream, "SOCKET-RECV-ERROR", xsink);
        if (*xsink) {
            return;
        }
        is_pollable = output_fd >= 0;
    }

    DLLLOCAL void deref(ExceptionSink* xsink) override {
        if (ROdereference()) {
            poll_state.reset();
            if (output_stream && !need_reassign) {
                output_stream->unassignThread(xsink);
            }
            output_stream = nullptr;
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return phase == Phase::Done;
    }

    DLLLOCAL virtual void abort(ExceptionSink* xsink) override {
        if (output_stream && !need_reassign) {
            output_stream->unassignThread(xsink);
        }
        bool close_socket = bytes_received > 0 || current_chunk;
        output_stream = nullptr;
        current_chunk = nullptr;
        poll_state.reset();
        if (close_socket) {
            qore_socket_close_from_controller(sock);
        }
        phase = Phase::Error;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (need_reassign) {
            need_reassign = false;
            if (output_stream) {
                output_stream->reassignThread(xsink);
                if (*xsink) {
                    phase = Phase::Error;
                    return nullptr;
                }
            }
        }

        qore_socket_private* priv = qore_socket_private::get(*sock);
        if (phase == Phase::RecvChunk && size < 0 && bytes_received > 0 && !priv->isOpen()) {
            complete(xsink);
            return nullptr;
        }
        if (!priv->isOpen()) {
            se_not_open("Socket", "recv", xsink);
            phase = Phase::Error;
            return nullptr;
        }
        if (checkTimeout(xsink)) {
            return nullptr;
        }

        unsigned loop = 0;
        while (true) {
            switch (phase) {
                case Phase::RecvChunk: {
                    if (size >= 0 && bytes_received >= size) {
                        complete(xsink);
                        return nullptr;
                    }

                    int64 chunk_size = getNextChunkSize();
                    if (chunk_size <= 0) {
                        complete(xsink);
                        return nullptr;
                    }

                    if (!poll_state) {
                        poll_state.reset(sock->startRecvSome(xsink, static_cast<size_t>(chunk_size)));
                        if (*xsink || !poll_state) {
                            phase = Phase::Error;
                            return nullptr;
                        }
                    }

                    int rc = poll_state->continuePoll(xsink);
                    if (*xsink) {
                        phase = Phase::Error;
                        poll_state.reset();
                        return nullptr;
                    }
                    if (rc) {
                        return getPollInfo(xsink, rc);
                    }

                    SimpleRefHolder<BinaryNode> chunk(poll_state->takeOutput().get<BinaryNode>());
                    poll_state.reset();
                    if (!chunk || !chunk->size()) {
                        if (size >= 0) {
                            xsink->raiseException("SOCKET-RECV-ERROR", "Unexpected end of stream");
                            phase = Phase::Error;
                            return nullptr;
                        }
                        complete(xsink);
                        return nullptr;
                    }

                    bytes_received += chunk->size();
                    qore_socket_private::get(*sock)->do_data_event(
                        QORE_EVENT_SOCKET_DATA_READ, QORE_SOURCE_SOCKET, **chunk);
                    current_chunk = chunk.release();
                    write_offset = 0;
                    clearTimeout();
                    phase = Phase::WriteChunk;
                    continue;
                }

                case Phase::WriteChunk: {
                    while (write_offset < current_chunk->size()) {
                        if (is_pollable) {
                            assert(output_fd >= 0);
                            int poll_rv = qore_socket_poll_fd_now(output_fd, POLLOUT, "SOCKET-RECV-ERROR",
                                "output stream", xsink);
                            if (poll_rv < 0) {
                                phase = Phase::Error;
                                return nullptr;
                            }
                            if (!poll_rv) {
                                return getPollInfo(xsink, SOCK_POLLIN, true);
                            }
                        }

                        const char* ptr = reinterpret_cast<const char*>(current_chunk->getPtr()) + write_offset;
                        size_t remaining = current_chunk->size() - write_offset;
                        int64 written = output_stream->writeNonBlock(ptr, remaining, xsink);
                        if (*xsink || written < 0) {
                            phase = Phase::Error;
                            return nullptr;
                        }
                        if (!written) {
                            return getPollInfo(xsink, SOCK_POLLIN, true);
                        }

                        write_offset += static_cast<size_t>(written);
                        clearTimeout();
                        if (++loop >= max_nonblock_ops && write_offset < current_chunk->size()) {
                            return getPollInfo(xsink, SOCK_POLLIN, true);
                        }
                    }

                    current_chunk = nullptr;
                    write_offset = 0;
                    if (++loop >= max_nonblock_ops && (size < 0 || bytes_received < size)) {
                        return getPollInfo(xsink, SOCK_POLLIN);
                    }
                    phase = Phase::RecvChunk;
                    continue;
                }

                case Phase::Done:
                case Phase::Error:
                    return nullptr;
            }
        }
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        switch (phase) {
            case Phase::RecvChunk: return "receiving-chunk";
            case Phase::WriteChunk: return "writing-chunk";
            case Phase::Done: return "done";
            case Phase::Error: return "error";
            default: return "unknown";
        }
    }

private:
    enum class Phase { RecvChunk, WriteChunk, Done, Error };

    DLLLOCAL QoreHashNode* getPollInfo(ExceptionSink* xsink, int events, bool output_wait = false) {
        int64 poll_timeout_ms = -1;
        if (timeout_ms >= 0) {
            int us;
            int64 now_s = q_epoch_us(us);
            int64 now_ms = now_s * 1000 + us / 1000;
            if (!wait_deadline_ms) {
                wait_deadline_ms = now_ms + timeout_ms;
            }
            poll_timeout_ms = wait_deadline_ms - now_ms;
            if (poll_timeout_ms < 0) {
                poll_timeout_ms = 0;
            }
        }

        QoreHashNode* raw_info = nullptr;
        if (output_wait && is_pollable && output_fd >= 0) {
            std::vector<std::pair<int, int>> extra_fds{{output_fd, SOCK_POLLOUT}};
            raw_info = getSocketPollInfoHash(xsink, 0, extra_fds);
        } else {
            raw_info = getSocketPollInfoHash(xsink, events);
        }
        if (!raw_info) {
            return nullptr;
        }
        ReferenceHolder<QoreHashNode> info(raw_info, xsink);
        if (poll_timeout_ms >= 0) {
            info->setKeyValue("poll_timeout_ms", poll_timeout_ms, xsink);
        }
        return info.release();
    }

    DLLLOCAL int64 getNextChunkSize() const {
        if (size < 0) {
            return DEFAULT_SOCKET_BUFSIZE;
        }
        return QORE_MIN(size - bytes_received, (int64)DEFAULT_SOCKET_BUFSIZE);
    }

    DLLLOCAL void complete(ExceptionSink* xsink) {
        if (output_stream && !need_reassign) {
            output_stream->unassignThread(xsink);
            if (*xsink) {
                phase = Phase::Error;
                return;
            }
        }
        output_stream = nullptr;
        phase = Phase::Done;
    }

    DLLLOCAL bool checkTimeout(ExceptionSink* xsink) {
        if (wait_deadline_ms <= 0) {
            return false;
        }
        int us;
        int64 now_s = q_epoch_us(us);
        int64 now_ms = now_s * 1000 + us / 1000;
        if (now_ms < wait_deadline_ms) {
            return false;
        }
        xsink->raiseException("SOCKET-TIMEOUT", "socket operation timed out");
        phase = Phase::Error;
        return true;
    }

    DLLLOCAL void clearTimeout() {
        wait_deadline_ms = 0;
    }

    QoreSocket* sock;
    std::unique_ptr<AbstractPollState> poll_state;
    SimpleRefHolder<OutputStream> output_stream;
    int64 size = -1;
    int64 bytes_received = 0;
    int timeout_ms = -1;
    int64 wait_deadline_ms = 0;
    int output_fd = -1;
    bool is_pollable = true;
    bool need_reassign = true;
    Phase phase = Phase::RecvChunk;
    SimpleRefHolder<BinaryNode> current_chunk;
    size_t write_offset = 0;
};

class QoreSocketControllerAcceptPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketControllerAcceptPollOperation(AbstractPollState* poll_state) : poll_state(poll_state) {
    }

    DLLLOCAL QoreSocketControllerAcceptPollOperation(QoreSocket* sock, SocketSource* source)
            : sock(sock), source(source) {
    }

    DLLLOCAL QoreSocketControllerAcceptPollOperation(QoreSocket* sock, SocketSource* source, bool ssl,
            QoreSSLCertificate* cert, QoreSSLPrivateKey* pkey)
            : sock(sock), source(source), ssl(ssl), cert(cert ? cert->certRefSelf() : nullptr),
            pkey(pkey ? pkey->pkRefSelf() : nullptr) {
    }

    DLLLOCAL ~QoreSocketControllerAcceptPollOperation() {
        discardAcceptedPollable();
    }

    DLLLOCAL virtual bool goalReached() const override {
        return state == SPS_ACCEPTED;
    }

    DLLLOCAL virtual void abort(ExceptionSink* xsink) override {
        poll_state.reset();
        discardAcceptedPollable(xsink);
        accepted_socket.reset();
        state = SPS_NONE;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        while (true) {
            if (!poll_state) {
                if (state == SPS_ACCEPTING) {
                    if (!sock) {
                        return nullptr;
                    }
                    poll_state.reset(new SocketAcceptPollState(xsink, qore_socket_private::get(*sock), source));
                } else if (state == SPS_ACCEPTING_SSL) {
                    assert(accepted_socket);
                    poll_state.reset(accepted_socket->startSslAccept(xsink, *cert, *pkey));
                } else {
                    return nullptr;
                }
                if (*xsink || !poll_state) {
                    state = SPS_NONE;
                    return nullptr;
                }
            }

            int rc = poll_state->continuePoll(xsink);
            if (*xsink || rc < 0) {
                poll_state.reset();
                state = SPS_NONE;
                return nullptr;
            }
            if (!rc) {
                if (state == SPS_ACCEPTING) {
                    assert(dynamic_cast<SocketAcceptPollState*>(poll_state.get()));
                    int descriptor = reinterpret_cast<SocketAcceptPollState*>(poll_state.get())->getDescriptor();
                    poll_state.reset();
                    accept_completed = true;
                    accepted_socket.reset(sock->createAcceptedSocket(descriptor));
                    if (ssl) {
                        state = SPS_ACCEPTING_SSL;
                        bumpFdGeneration();
                        continue;
                    }
                } else {
                    poll_state.reset();
                    discardAcceptedPollable(xsink);
                }
                state = SPS_ACCEPTED;
                return nullptr;
            }
            return state == SPS_ACCEPTING_SSL
                ? getAcceptedSocketPollInfoHash(xsink, rc)
                : getSocketPollInfoHash(xsink, rc);
        }
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return static_cast<bool>(accepted_socket);
    }

    DLLLOCAL QoreSocket* releaseAcceptedSocket() {
        return accepted_socket.release();
    }

    DLLLOCAL bool hasAcceptedConnection() const {
        return accept_completed;
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        switch (state) {
            case SPS_ACCEPTING:
                return "accepting";
            case SPS_ACCEPTING_SSL:
                return "accepting-ssl";
            case SPS_ACCEPTED:
                return "accepted";
            default:
                return "aborted";
        }
    }

private:
    DLLLOCAL QoreHashNode* getAcceptedSocketPollInfoHash(ExceptionSink* xsink, int events) {
        ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollInfo, xsink), xsink);
        info->setKeyValue("events", events, xsink);
        if (!accepted_pollable_obj) {
            accepted_pollable_obj = new QoreObject(QC_ABSTRACTPOLLABLEIOOBJECTBASE, getProgram(),
                new QoreSocketControllerPollable(accepted_socket.get()));
        }
        info->setKeyValue("socket", accepted_pollable_obj->objectRefSelf(), xsink);
        return info.release();
    }

    DLLLOCAL void discardAcceptedPollable(ExceptionSink* xsink = nullptr) {
        if (!accepted_pollable_obj) {
            return;
        }
        ExceptionSink local_xsink;
        accepted_pollable_obj->deref(xsink ? xsink : &local_xsink);
        if (local_xsink) {
            local_xsink.clear();
        }
        accepted_pollable_obj = nullptr;
    }

    QoreSocket* sock = nullptr;
    SocketSource* source = nullptr;
    std::unique_ptr<AbstractPollState> poll_state;
    mutable std::unique_ptr<QoreSocket> accepted_socket;
    QoreObject* accepted_pollable_obj = nullptr;
    int state = SPS_ACCEPTING;
    bool ssl = false;
    SimpleRefHolder<QoreSSLCertificate> cert;
    SimpleRefHolder<QoreSSLPrivateKey> pkey;
    bool accept_completed = false;
};

class QoreSocketControllerHttp2SendResponsePollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketControllerHttp2SendResponsePollOperation(QoreSocket* sock, int32_t stream_id, int status_code,
            const QoreHashNode* headers, const void* data, size_t size, const QoreStringNode* body_event)
            : sock(sock), stream_id(stream_id), status_code(status_code) {
        if (headers) {
            ConstHashIterator hi(headers);
            while (hi.next()) {
                const char* key = hi.getKey();
                QoreValue val = hi.get();
                if (val.getType() == NT_STRING) {
                    hdr_pairs.emplace_back(key, val.get<const QoreStringNode>()->c_str());
                } else if (val.getType() == NT_LIST) {
                    const QoreListNode* l = val.get<const QoreListNode>();
                    for (size_t i = 0; i < l->size(); ++i) {
                        QoreValue lv = l->retrieveEntry(i);
                        if (lv.getType() == NT_STRING) {
                            hdr_pairs.emplace_back(key, lv.get<const QoreStringNode>()->c_str());
                        }
                    }
                }
            }
        }

        if (data && size) {
            body = new BinaryNode;
            body->append(data, size);
        } else if (body_event && body_event->size()) {
            body = new BinaryNode;
            body->append(body_event->c_str(), body_event->size());
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return h2_state == H2S_SENT;
    }

    DLLLOCAL virtual void abort(ExceptionSink*) override {
        bool close_socket = h2_state == H2S_SENDING || h2_state == H2S_FLUSHING;
        h2_state = H2S_NONE;
        if (close_socket) {
            qore_socket_close_from_controller(sock);
        }
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        qore_socket_private* priv = qore_socket_private::get(*sock);
        if (!priv->isOpen()) {
            xsink->raiseException("HTTP2-ERROR", "socket closed during poll operation");
            return nullptr;
        }

        Http2Session* session = priv->h2_session.get();
        if (!session) {
            xsink->raiseException("HTTP2-ERROR", "HTTP/2 session no longer available");
            return nullptr;
        }

        OptionalNonBlockingHelper nbh(*priv, true, xsink);
        if (*xsink) {
            return nullptr;
        }

        while (true) {
            switch (h2_state) {
                case H2S_NONE: {
                    const void* body_ptr = body ? body->getPtr() : nullptr;
                    size_t body_len = body ? body->size() : 0;
                    int rv = session->submitResponse(stream_id, status_code, hdr_pairs, body_ptr, body_len, xsink);
                    if (rv || *xsink) {
                        return nullptr;
                    }
                    h2_state = H2S_SENDING;
                    continue;
                }

                case H2S_SENDING: {
                    int rv = session->sendPendingData(0, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (rv == SOCK_POLLIN || rv == SOCK_POLLOUT) {
                        return getSocketPollInfoHash(xsink, rv);
                    }
                    if (session->hasPendingData()) {
                        return getSocketPollInfoHash(xsink, SOCK_POLLOUT);
                    }
                    if (session->wantWrite()) {
                        continue;
                    }
                    h2_state = H2S_FLUSHING;
                    return getSocketPollInfoHash(xsink, SOCK_POLLOUT);
                }

                case H2S_FLUSHING: {
                    int rv = session->sendPendingData(0, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (rv == SOCK_POLLIN || rv == SOCK_POLLOUT) {
                        return getSocketPollInfoHash(xsink, rv);
                    }
                    if (session->hasPendingData()) {
                        return getSocketPollInfoHash(xsink, SOCK_POLLOUT);
                    }
                    if (session->wantWrite()) {
                        h2_state = H2S_SENDING;
                        continue;
                    }
                    h2_state = H2S_SENT;
                    return nullptr;
                }

                case H2S_SENT:
                    return nullptr;

                default:
                    xsink->raiseException("HTTP2-ERROR", "invalid HTTP/2 send response state: %d", h2_state);
                    return nullptr;
            }
        }
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        switch (h2_state) {
            case H2S_NONE: return "none";
            case H2S_SENDING: return "sending";
            case H2S_FLUSHING: return "flushing";
            case H2S_SENT: return "sent";
            default: return "unknown";
        }
    }

private:
    QoreSocket* sock;
    int32_t stream_id;
    int status_code;
    std::vector<std::pair<std::string, std::string>> hdr_pairs;
    SimpleRefHolder<BinaryNode> body;
    int h2_state = H2S_NONE;
};

class QoreSocketControllerHttp2EnqueuePollOperation : public SocketPollOperationBase {
public:
    enum class Action {
        PushPromise,
        Response,
        ConnectResponse,
        Request,
        Cancel,
        StreamData,
        Trailers,
    };

    DLLLOCAL QoreSocketControllerHttp2EnqueuePollOperation(QoreSocket* sock, Action action,
            int32_t stream_id, const char* path, const QoreHashNode* headers)
            : sock(sock), action(action), stream_id(stream_id), path(path ? path : "") {
        setHeaders(header_map, headers);
    }

    DLLLOCAL QoreSocketControllerHttp2EnqueuePollOperation(QoreSocket* sock, Action action,
            int32_t stream_id, int status_code, const QoreHashNode* headers, const void* body_ptr = nullptr,
            size_t body_len = 0)
            : sock(sock), action(action), stream_id(stream_id), status_code(status_code) {
        setHeaders(header_map, headers);
        if (body_ptr && body_len) {
            body = new BinaryNode;
            body->append(body_ptr, body_len);
        }
    }

    DLLLOCAL QoreSocketControllerHttp2EnqueuePollOperation(QoreSocket* sock, const QoreHashNode* headers,
            const void* body_ptr, size_t body_len, bool streaming, ExceptionSink* xsink)
            : sock(sock), action(Action::Request), streaming(streaming) {
        parseRequestHeaders(headers, xsink);
        if (body_ptr && body_len) {
            body = new BinaryNode;
            body->append(body_ptr, body_len);
        }
    }

    DLLLOCAL QoreSocketControllerHttp2EnqueuePollOperation(QoreSocket* sock, int32_t stream_id,
            const BinaryNode* data, bool end_stream)
            : sock(sock), action(Action::StreamData), stream_id(stream_id), end_stream(end_stream) {
        if (data && data->size()) {
            body = new BinaryNode;
            body->append(data->getPtr(), data->size());
        }
    }

    DLLLOCAL QoreSocketControllerHttp2EnqueuePollOperation(QoreSocket* sock, Action action,
            int32_t stream_id)
            : sock(sock), action(action), stream_id(stream_id) {
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

        qore_socket_private* priv = qore_socket_private::get(*sock);
        Http2Session* h2 = priv->h2_session.get();
        if (!h2) {
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
                } else {
                    output = rv;
                }
                break;
            }

            case Action::Trailers:
                output = h2->submitTrailers(stream_id, header_map, xsink);
                break;
        }

        if (!*xsink && output >= 0) {
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
    DLLLOCAL static void setHeaders(strcase_str_map_t& out, const QoreHashNode* headers) {
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

    DLLLOCAL void parseRequestHeaders(const QoreHashNode* headers, ExceptionSink* xsink) {
        if (!headers) {
            xsink->raiseException("HTTP2-ERROR", "HTTP/2 request headers are required");
            return;
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
            return;
        }
        if (path.empty()) {
            xsink->raiseException("HTTP2-ERROR", "HTTP/2 request ':path' header is missing or empty");
        }
    }

    QoreSocket* sock;
    Action action;
    int32_t stream_id = 0;
    int status_code = 0;
    std::string method;
    std::string path;
    strcase_str_map_t header_map;
    std::vector<std::pair<std::string, std::string>> request_headers;
    SimpleRefHolder<BinaryNode> body;
    bool end_stream = false;
    bool streaming = false;
    int64 output = -1;
    bool done = false;
};

class QoreSocketControllerHttp2StreamDataPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketControllerHttp2StreamDataPollOperation(QoreSocket* sock, int32_t stream_id,
            size_t max_bytes)
            : sock(sock), stream_id(stream_id), max_bytes(max_bytes) {
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

        qore_socket_private* priv = qore_socket_private::get(*sock);
        Http2Session* h2 = priv->h2_session.get();
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
    QoreSocket* sock;
    SimpleRefHolder<BinaryNode> data;
    int32_t stream_id;
    size_t max_bytes;
    bool done = false;
};

class QoreSocketControllerHttp2StreamStatePollOperation : public SocketPollOperationBase {
public:
    enum class Action {
        Closed,
        RemoteClosed,
    };

    DLLLOCAL QoreSocketControllerHttp2StreamStatePollOperation(QoreSocket* sock, int32_t stream_id,
            Action action)
            : sock(sock), stream_id(stream_id), action(action) {
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

        qore_socket_private* priv = qore_socket_private::get(*sock);
        Http2Session* h2 = priv->h2_session.get();
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
    QoreSocket* sock;
    int32_t stream_id;
    Action action;
    bool output = true;
    bool done = false;
};

class QoreSocketControllerTlsStatePollOperation : public SocketPollOperationBase {
public:
    enum class Action {
        GetCipherName,
        GetCipherVersion,
        IsSecure,
        SetAlpnProtocols,
        ClearAlpnProtocols,
        GetAlpnProtocol,
        IsHttp2,
        VerifyPeerCertificate,
        GetRemoteCertificate,
    };

    DLLLOCAL QoreSocketControllerTlsStatePollOperation(QoreSocket* sock, Action action)
            : sock(sock), action(action) {
    }

    DLLLOCAL QoreSocketControllerTlsStatePollOperation(QoreSocket* sock, std::vector<std::string>&& protocols)
            : sock(sock), action(Action::SetAlpnProtocols), protocols(std::move(protocols)) {
    }

    DLLLOCAL ~QoreSocketControllerTlsStatePollOperation() override {
        ExceptionSink xsink;
        output.discard(&xsink);
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

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink*) override {
        if (done) {
            return nullptr;
        }

        qore_socket_private* priv = qore_socket_private::get(*sock);
        switch (action) {
            case Action::GetCipherName:
                setStringValue(priv->ssl ? priv->ssl->getCipherName() : nullptr);
                break;

            case Action::GetCipherVersion:
                setStringValue(priv->ssl ? priv->ssl->getCipherVersion() : nullptr);
                break;

            case Action::IsSecure:
                output = static_cast<bool>(priv->ssl);
                break;

            case Action::SetAlpnProtocols:
                priv->alpn_protocols = protocols;
                output = 0;
                break;

            case Action::ClearAlpnProtocols:
                priv->alpn_protocols.clear();
                output = 0;
                break;

            case Action::GetAlpnProtocol:
                setAlpnValue(priv);
                break;

            case Action::IsHttp2:
                output = priv->ssl ? priv->ssl->isHttp2() : false;
                break;

            case Action::VerifyPeerCertificate:
                output = priv->ssl ? static_cast<int64>(priv->ssl->verifyPeerCertificate()) : static_cast<int64>(-1);
                break;

            case Action::GetRemoteCertificate:
                if (priv->remote_cert) {
                    priv->remote_cert->ref();
                    output = priv->remote_cert;
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
            case Action::SetAlpnProtocols:
            case Action::ClearAlpnProtocols:
            case Action::VerifyPeerCertificate:
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

    DLLLOCAL void setAlpnValue(qore_socket_private* priv) {
        if (!priv->ssl) {
            return;
        }
        std::string proto = priv->ssl->getAlpnProtocol();
        if (!proto.empty()) {
            output = new QoreStringNode(proto.c_str());
        }
    }

    QoreSocket* sock;
    QoreValue output;
    Action action;
    std::vector<std::string> protocols;
    bool done = false;
};

static QoreHashNode* qore_socket_exec_poll_operation(QoreObject* sock_obj, AbstractPollableIoObjectBase* pollable,
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

    const std::string& socket_hash = pollable->getIoIdentityHash();

    std::string owner("QoreSocket::sync:");
    owner += owner_name;
    owner += ':';
    owner += socket_hash;

    std::string key("QoreSocket::sync:");
    key += owner_name;
    key += ':';
    key += socket_hash;
    // Several synchronous callers can delegate work for the same socket at the
    // same time; keep the cache key unique while thread_key preserves affinity.
    key += ':';
    key += std::to_string(++qore_socket_sync_exec_seq);

    ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollOperationInfo, xsink), xsink);
    info->setKeyValue("sock", sock_obj->objectRefSelf(), xsink);
    info->setKeyValue("spop", op_obj->objectRefSelf(), xsink);
    info->setKeyValue("owner", new QoreStringNode(owner), xsink);
    info->setKeyValue("key", new QoreStringNode(key), xsink);
    info->setKeyValue("thread_key", new QoreStringNode(socket_hash), xsink);
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
            return result.release();
        }
        qore_socket_raise_poll_result_exception(ex.get<const QoreHashNode>(), xsink);
        return nullptr;
    }
    return result.release();
}

static QoreValue qore_socket_exec_poll(QoreSocket* s, SocketPollOperationBase* poller, int timeout_ms,
        const char* owner_name, const char* goal, ExceptionSink* xsink, QoreHashNode** ex_out = nullptr) {
    ReferenceHolder<SocketPollOperationBase> poller_holder(poller, xsink);
    if (*xsink) {
        return QoreValue();
    }

    QoreSocketControllerPollable* pollable = new QoreSocketControllerPollable(s);
    ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_ABSTRACTPOLLABLEIOOBJECTBASE, getProgram(), pollable),
        xsink);
    ReferenceHolder<QoreObject> op_obj(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), *poller_holder),
        xsink);
    poller_holder->setSelf(*op_obj);
    poller_holder.release();

    if (!*xsink) {
        op_obj->setValue("sock", (*sock_obj)->objectRefSelf(), xsink);
        op_obj->setValue("goal", new QoreStringNode(goal), xsink);
    }
    if (*xsink) {
        return QoreValue();
    }

    ReferenceHolder<QoreHashNode> result(
        qore_socket_exec_poll_operation(*sock_obj, pollable, *op_obj, timeout_ms, owner_name, xsink, ex_out),
        xsink);
    if (*xsink) {
        return QoreValue();
    }
    return poller->getOutput();
}

static QoreStringNode* qore_socket_exec_tls_state_string(QoreSocket* s,
        QoreSocketControllerTlsStatePollOperation::Action action, const char* owner_name, ExceptionSink* xsink) {
    ValueHolder rv(qore_socket_exec_poll(s, new QoreSocketControllerTlsStatePollOperation(s, action), -1,
        owner_name, "tls-state", xsink), xsink);
    if (*xsink || rv->isNothing()) {
        return nullptr;
    }
    return rv->take<QoreStringNode>();
}

static bool qore_socket_exec_tls_state_bool(QoreSocket* s,
        QoreSocketControllerTlsStatePollOperation::Action action, const char* owner_name, ExceptionSink* xsink) {
    ValueHolder rv(qore_socket_exec_poll(s, new QoreSocketControllerTlsStatePollOperation(s, action), -1,
        owner_name, "tls-state", xsink), xsink);
    return *xsink ? false : rv->getAsBool();
}

static int64 qore_socket_exec_tls_state_int(QoreSocket* s,
        QoreSocketControllerTlsStatePollOperation::Action action, const char* owner_name, ExceptionSink* xsink) {
    ValueHolder rv(qore_socket_exec_poll(s, new QoreSocketControllerTlsStatePollOperation(s, action), -1,
        owner_name, "tls-state", xsink), xsink);
    return *xsink ? -1 : rv->getAsBigInt();
}

static QoreObject* qore_socket_exec_tls_state_object(QoreSocket* s,
        QoreSocketControllerTlsStatePollOperation::Action action, const char* owner_name, ExceptionSink* xsink) {
    ValueHolder rv(qore_socket_exec_poll(s, new QoreSocketControllerTlsStatePollOperation(s, action), -1,
        owner_name, "tls-state", xsink), xsink);
    if (*xsink || rv->isNothing()) {
        return nullptr;
    }
    return rv->take<QoreObject>();
}

static int qore_socket_exec_tls_state_no_output(QoreSocket* s, QoreSocketControllerTlsStatePollOperation* poller,
        const char* owner_name, ExceptionSink* xsink) {
    ValueHolder rv(qore_socket_exec_poll(s, poller, -1, owner_name, "tls-state", xsink), xsink);
    return *xsink ? -1 : 0;
}

static int64 qore_socket_exec_http2_enqueue_int(QoreSocket* s,
        QoreSocketControllerHttp2EnqueuePollOperation* poller, const char* owner_name, ExceptionSink* xsink) {
    ValueHolder rv(qore_socket_exec_poll(s, poller, -1, owner_name, "done", xsink), xsink);
    if (*xsink) {
        return -1;
    }
    return rv->isNothing() ? -1 : rv->getAsBigInt();
}

static BinaryNode* qore_socket_exec_http2_stream_data(QoreSocket* s,
        QoreSocketControllerHttp2StreamDataPollOperation* poller, int32_t stream_id, ExceptionSink* xsink) {
    ValueHolder rv(qore_socket_exec_poll(s, poller, -1, "readHttp2StreamData", "http2-stream-data", xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    if (rv->isNothing()) {
        return nullptr;
    }
    if (rv->getType() != NT_BINARY) {
        xsink->raiseException("HTTP2-ERROR", "unexpected %s value reading HTTP/2 stream data",
            rv->getFullTypeName());
        return nullptr;
    }
    return rv.release().get<BinaryNode>();
}

static bool qore_socket_exec_http2_stream_state(QoreSocket* s,
        QoreSocketControllerHttp2StreamStatePollOperation* poller, const char* owner_name, ExceptionSink* xsink) {
    ValueHolder rv(qore_socket_exec_poll(s, poller, -1, owner_name, "http2-stream-state", xsink), xsink);
    if (*xsink || rv->isNothing()) {
        return true;
    }
    return rv->getAsBool();
}

static QoreSocket* qore_socket_exec_accept_socket(QoreSocket* s, SocketSource* source, int timeout_ms,
        ExceptionSink* xsink) {
    qore_socket_private* priv = qore_socket_private::get(*s);
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_ALL);
    if (!io_guard) {
        return nullptr;
    }

    QoreSocketControllerAcceptPollOperation* accept_poller = new QoreSocketControllerAcceptPollOperation(s, source);
    ReferenceHolder<SocketPollOperationBase> poller_holder(accept_poller, xsink);
    if (*xsink) {
        return nullptr;
    }

    QoreSocketControllerPollable* pollable = new QoreSocketControllerPollable(s);
    ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_ABSTRACTPOLLABLEIOOBJECTBASE, getProgram(), pollable),
        xsink);
    ReferenceHolder<QoreObject> op_obj(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), *poller_holder),
        xsink);
    poller_holder->setSelf(*op_obj);
    poller_holder.release();

    if (!*xsink) {
        op_obj->setValue("sock", (*sock_obj)->objectRefSelf(), xsink);
        op_obj->setValue("goal", new QoreStringNode("accepted"), xsink);
    }
    if (*xsink) {
        return nullptr;
    }

    QoreHashNode* ex = nullptr;
    ReferenceHolder<QoreHashNode> result(qore_socket_exec_poll_operation(*sock_obj, pollable, *op_obj, timeout_ms,
        "accept", xsink, &ex), xsink);
    ReferenceHolder<QoreHashNode> ex_holder(ex, xsink);
    if (*xsink) {
        return nullptr;
    }
    if (ex_holder) {
        if (qore_socket_exec_exception_is(**ex_holder, "SOCKET-TIMEOUT")) {
            return nullptr;
        }
        qore_socket_raise_poll_result_exception(*ex_holder, xsink);
        return nullptr;
    }

    QoreSocket* rv = accept_poller->releaseAcceptedSocket();
    if (!rv) {
        xsink->raiseException("SOCKET-ACCEPT-ERROR",
            "async accept operation did not return an accepted socket");
    }
    return rv;
}

static QoreSocket* qore_socket_exec_accept_ssl_socket(QoreSocket* s, SocketSource* source, int timeout_ms,
        QoreSSLCertificate* cert, QoreSSLPrivateKey* pkey, ExceptionSink* xsink) {
    qore_socket_private* priv = qore_socket_private::get(*s);
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_ALL);
    if (!io_guard) {
        return nullptr;
    }

    QoreSocketControllerAcceptPollOperation* accept_poller = new QoreSocketControllerAcceptPollOperation(s, source,
        true, cert, pkey);
    ReferenceHolder<SocketPollOperationBase> poller_holder(accept_poller, xsink);
    if (*xsink) {
        return nullptr;
    }

    QoreSocketControllerPollable* pollable = new QoreSocketControllerPollable(s);
    ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_ABSTRACTPOLLABLEIOOBJECTBASE, getProgram(), pollable),
        xsink);
    ReferenceHolder<QoreObject> op_obj(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), *poller_holder),
        xsink);
    poller_holder->setSelf(*op_obj);
    poller_holder.release();

    if (!*xsink) {
        op_obj->setValue("sock", (*sock_obj)->objectRefSelf(), xsink);
        op_obj->setValue("goal", new QoreStringNode("accept-ssl"), xsink);
    }
    if (*xsink) {
        return nullptr;
    }

    QoreHashNode* ex = nullptr;
    ReferenceHolder<QoreHashNode> result(qore_socket_exec_poll_operation(*sock_obj, pollable, *op_obj, timeout_ms,
        "acceptSSL", xsink, &ex), xsink);
    ReferenceHolder<QoreHashNode> ex_holder(ex, xsink);
    if (*xsink) {
        return nullptr;
    }
    if (ex_holder) {
        if (qore_socket_exec_exception_is(**ex_holder, "SOCKET-TIMEOUT")
                && !accept_poller->hasAcceptedConnection()) {
            return nullptr;
        }
        qore_socket_raise_poll_result_exception(*ex_holder, xsink);
        return nullptr;
    }

    QoreSocket* rv = accept_poller->releaseAcceptedSocket();
    if (!rv) {
        xsink->raiseException("SOCKET-ACCEPT-ERROR",
            "async acceptSSL operation did not return an accepted socket");
    }
    return rv;
}

QoreSocket* QoreSocket::createAcceptedSocket(int descriptor) {
    QoreSocket* s = new QoreSocket(descriptor, priv->sfamily, priv->stype, priv->sprot, priv->enc);
    if (!priv->socketname.empty()) {
        s->priv->socketname = priv->socketname;
    }

    s->priv->setSslVerifyMode(priv->ssl_verify_mode);
    s->priv->acceptAllCertificates(priv->ssl_accept_all_certs);
    if (priv->ssl_capture_remote_cert) {
        s->priv->ssl_capture_remote_cert = true;
    }
    if (!priv->alpn_protocols.empty()) {
        s->priv->alpn_protocols = priv->alpn_protocols;
    }
    return s;
}

static int qore_socket_exec_send_bytes(QoreSocket* s, const void* data, size_t size, int timeout_ms,
        ExceptionSink* xsink, int source = QORE_SOURCE_SOCKET, const char* http1_method = nullptr) {
    if (!size) {
        return 0;
    }

    qore_socket_private* priv = qore_socket_private::get(*s);
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_SEND);
    if (!io_guard) {
        return -1;
    }

    ValueHolder rv(qore_socket_exec_poll(s,
        new QoreSocketControllerSendBytesPollOperation(s, data, size, http1_method),
        timeout_ms, "send", "send", xsink), xsink);
    if (*xsink) {
        return -1;
    }
    if (source > 0) {
        priv->do_data_event(QORE_EVENT_SOCKET_DATA_SENT, source, data, size);
    }
    return 0;
}

static int qore_socket_exec_send_bytes_no_exception(QoreSocket* s, const void* data, size_t size, int timeout_ms) {
    ExceptionSink xsink;
    int rc = qore_socket_exec_send_bytes(s, data, size, timeout_ms, &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

static BinaryNode* qore_socket_exec_recv_binary(QoreSocket* s, qore_offset_t size, int timeout_ms,
        ExceptionSink* xsink, int source = QORE_SOURCE_SOCKET) {
    if (size > std::numeric_limits<size_t>::max()) {
        xsink->raiseException("SOCKET-RECV-ERROR",
            "requested receive size " QLLD " exceeds the maximum supported size",
            static_cast<int64>(size));
        return nullptr;
    }

    qore_socket_private* priv = qore_socket_private::get(*s);
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_RECV);
    if (!io_guard) {
        return nullptr;
    }

    ValueHolder result(qore_socket_exec_poll(s,
        new QoreSocketControllerRecvPollOperation(s,
            size > 0 ? QoreSocketControllerRecvPollOperation::Action::Recv
                : QoreSocketControllerRecvPollOperation::Action::RecvPacket,
            size > 0 ? static_cast<size_t>(size) : 0, false, "receiving", "received"),
        timeout_ms, "recv", "received", xsink), xsink);
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

    BinaryNode* bin = result.release().get<BinaryNode>();
    if (source > 0) {
        priv->do_data_event(QORE_EVENT_SOCKET_DATA_READ, source, *bin);
    }
    return bin;
}

static BinaryNode* qore_socket_exec_recv_some_binary(QoreSocket* s, size_t size, int timeout_ms,
        const char* owner_name, ExceptionSink* xsink, int source = QORE_SOURCE_SOCKET) {
    qore_socket_private* priv = qore_socket_private::get(*s);
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_RECV);
    if (!io_guard) {
        return nullptr;
    }

    ValueHolder result(qore_socket_exec_poll(s,
        new QoreSocketControllerRecvPollOperation(s, QoreSocketControllerRecvPollOperation::Action::RecvSome,
            size, false, "receiving", "received"),
        timeout_ms, owner_name, "received", xsink), xsink);
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

    BinaryNode* bin = result.release().get<BinaryNode>();
    if (source > 0) {
        priv->do_data_event(QORE_EVENT_SOCKET_DATA_READ, source, *bin);
    }
    return bin;
}

struct QoreSocketInputStreamRefGuard {
    QoreSocketInputStreamRefGuard(InputStream* is, ExceptionSink* xsink) : is(is), xsink(xsink) {
        this->is->ref();
    }

    ~QoreSocketInputStreamRefGuard() {
        is->deref(xsink);
    }

private:
    InputStream* is;
    ExceptionSink* xsink;
};

struct QoreSocketOutputStreamRefGuard {
    QoreSocketOutputStreamRefGuard(OutputStream* os, ExceptionSink* xsink) : os(os), xsink(xsink) {
        this->os->ref();
    }

    ~QoreSocketOutputStreamRefGuard() {
        os->deref(xsink);
    }

private:
    OutputStream* os;
    ExceptionSink* xsink;
};

static int qore_socket_exec_send_input_stream_poll(QoreSocket* s, InputStream* is, int64 size, int timeout_ms,
        ExceptionSink* xsink) {
    if (!size) {
        return 0;
    }
    if (!is->isIoThreadSafe()) {
        xsink->raiseException("SOCKET-SEND-ERROR", "InputStream is not I/O thread safe");
        return -1;
    }

    qore_socket_private* priv = qore_socket_private::get(*s);
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_SEND);
    if (!io_guard) {
        return -1;
    }

    QoreSocketInputStreamRefGuard caller_ref(is, xsink);
    is->ref();
    ReferenceHolder<SocketPollOperationBase> poller(
        new QoreSocketControllerSendInputStreamPollOperation(xsink, s, is, size, timeout_ms), xsink);
    if (*xsink) {
        return -1;
    }

    is->unassignThread(xsink);
    if (*xsink) {
        return -1;
    }

    ValueHolder result(qore_socket_exec_poll(s, poller.release(), -1, "send", "send", xsink), xsink);
    if (!*xsink) {
        is->reassignThread(xsink);
    } else {
        ExceptionSink reassign_xsink;
        is->reassignThread(&reassign_xsink);
        if (reassign_xsink) {
            reassign_xsink.clear();
        }
    }
    return *xsink ? -1 : 0;
}

static int qore_socket_exec_recv_output_stream_poll(QoreSocket* s, OutputStream* os, int64 size, int timeout_ms,
        ExceptionSink* xsink) {
    if (!size) {
        return 0;
    }
    if (!os->isIoThreadSafe()) {
        xsink->raiseException("SOCKET-RECV-ERROR", "OutputStream is not I/O thread safe");
        return -1;
    }

    qore_socket_private* priv = qore_socket_private::get(*s);
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_RECV);
    if (!io_guard) {
        return -1;
    }

    QoreSocketOutputStreamRefGuard caller_ref(os, xsink);
    os->ref();
    ReferenceHolder<SocketPollOperationBase> poller(
        new QoreSocketControllerRecvOutputStreamPollOperation(xsink, s, os, size, timeout_ms), xsink);
    if (*xsink) {
        return -1;
    }

    os->unassignThread(xsink);
    if (*xsink) {
        return -1;
    }

    ValueHolder result(qore_socket_exec_poll(s, poller.release(), -1, "recv", "received", xsink), xsink);
    if (!*xsink) {
        os->reassignThread(xsink);
    } else {
        ExceptionSink reassign_xsink;
        os->reassignThread(&reassign_xsink);
        if (reassign_xsink) {
            reassign_xsink.clear();
        }
    }
    return *xsink ? -1 : 0;
}

static QoreStringNode* qore_socket_exec_recv_string(QoreSocket* s, qore_offset_t size, int timeout_ms,
        ExceptionSink* xsink, int source = QORE_SOURCE_SOCKET) {
    if (size > std::numeric_limits<size_t>::max()) {
        xsink->raiseException("SOCKET-RECV-ERROR",
            "requested receive size " QLLD " exceeds the maximum supported size",
            static_cast<int64>(size));
        return nullptr;
    }

    qore_socket_private* priv = qore_socket_private::get(*s);
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_RECV);
    if (!io_guard) {
        return nullptr;
    }

    ValueHolder result(qore_socket_exec_poll(s,
        new QoreSocketControllerRecvPollOperation(s,
            size > 0 ? QoreSocketControllerRecvPollOperation::Action::Recv
                : QoreSocketControllerRecvPollOperation::Action::RecvPacket,
            size > 0 ? static_cast<size_t>(size) : 0, true, "receiving", "received"),
        timeout_ms, "recv", "received", xsink), xsink);
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

    QoreStringNode* str = result.release().get<QoreStringNode>();
    if (source > 0) {
        priv->do_data_event(QORE_EVENT_SOCKET_DATA_READ, source, *str);
    }
    return str;
}

static QoreStringNode* qore_socket_exec_recv_until_string(QoreSocket* s, const char* pattern, size_t pattern_size,
        int timeout_ms, const char* owner_name, ExceptionSink* xsink) {
    qore_socket_private* priv = qore_socket_private::get(*s);
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_RECV);
    if (!io_guard) {
        return nullptr;
    }

    ValueHolder result(qore_socket_exec_poll(s,
        new QoreSocketControllerRecvPollOperation(s, pattern, pattern_size, true, "receiving", "received"),
        timeout_ms, owner_name, "received", xsink), xsink);
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

static bool qore_socket_exec_is_data_available(QoreSocket* s, int timeout_ms, ExceptionSink* xsink) {
    qore_socket_private* priv = qore_socket_private::get(*s);
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_RECV);
    if (!io_guard) {
        return false;
    }

    QoreHashNode* ex = nullptr;
    ValueHolder result(qore_socket_exec_poll(s,
        new QoreSocketControllerDataAvailablePollOperation(s),
        timeout_ms, "isDataAvailable", "isDataAvailable", xsink, &ex), xsink);
    ReferenceHolder<QoreHashNode> ex_holder(ex, xsink);
    if (*xsink) {
        return false;
    }
    if (ex_holder) {
        if (qore_socket_exec_exception_is(**ex_holder, "SOCKET-TIMEOUT")) {
            return false;
        }
        qore_socket_raise_poll_result_exception(*ex_holder, xsink);
        return false;
    }
    return result->getAsBool();
}

static unsigned qore_socket_exec_readiness_direction(int events) {
    unsigned direction = 0;
    if (events & SOCK_POLLIN) {
        direction |= NB_RECV;
    }
    if (events & SOCK_POLLOUT) {
        direction |= NB_SEND;
    }
    return direction;
}

static int qore_socket_exec_wait_readiness(QoreSocket* s, int timeout_ms, int events, const char* owner_name,
        const char* waiting_state, const char* ready_state, ExceptionSink* xsink) {
    qore_socket_private* priv = qore_socket_private::get(*s);
    unsigned direction = qore_socket_exec_readiness_direction(events);
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, direction);
    if (direction && !io_guard) {
        return -1;
    }

    QoreHashNode* ex = nullptr;
    ValueHolder result(qore_socket_exec_poll(s,
        new QoreSocketControllerReadinessPollOperation(s, events, owner_name, waiting_state, ready_state),
        timeout_ms, owner_name, ready_state, xsink, &ex), xsink);
    ReferenceHolder<QoreHashNode> ex_holder(ex, xsink);
    if (*xsink) {
        return -1;
    }
    if (ex_holder) {
        if (qore_socket_exec_exception_is(**ex_holder, "SOCKET-TIMEOUT")) {
            return 0;
        }
        qore_socket_raise_poll_result_exception(*ex_holder, xsink);
        return -1;
    }
    return result->getAsBool() ? 1 : 0;
}

static int qore_socket_get_setup_rc(const QoreValue result, ExceptionSink* xsink) {
    if (result.isNothing()) {
        return -1;
    }
    if (result.getType() != NT_INT) {
        xsink->raiseException("SOCKET-SETUP-ERROR",
            "expected integer return code from async setup operation, got '%s'", result.getFullTypeName());
        return -1;
    }
    int64 rc = result.getAsBigInt();
    if (rc < std::numeric_limits<int>::min() || rc > std::numeric_limits<int>::max()) {
        xsink->raiseException("SOCKET-SETUP-ERROR", "invalid return code from async setup operation: " QLLD, rc);
        return -1;
    }
    return static_cast<int>(rc);
}

static int qore_socket_exec_setup(QoreSocket* s, QoreSocketControllerSetupPollOperation* poller,
        const char* owner_name, ExceptionSink* xsink) {
    ReferenceHolder<SocketPollOperationBase> poller_holder(poller, xsink);
    if (*xsink) {
        return -1;
    }

    bool config_action = poller->isConfigAction();
    if (!config_action) {
        qore_socket_private* priv = qore_socket_private::get(*s);
        QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_ALL);
        if (!io_guard) {
            return -1;
        }

        ValueHolder result(qore_socket_exec_poll(s, poller_holder.release(), -1, owner_name, "done", xsink), xsink);
        if (*xsink) {
            return -1;
        }
        return qore_socket_get_setup_rc(*result, xsink);
    }

    ValueHolder result(qore_socket_exec_poll(s, poller_holder.release(), -1, owner_name, "done", xsink), xsink);
    if (*xsink) {
        return -1;
    }
    return qore_socket_get_setup_rc(*result, xsink);
}

static int qore_socket_exec_setup_no_exception(QoreSocket* s, QoreSocketControllerSetupPollOperation* poller,
        const char* owner_name) {
    ExceptionSink xsink;
    int rc = qore_socket_exec_setup(s, poller, owner_name, &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

static QoreHashNode* qore_socket_get_addr_info_from_output(const QoreValue output, const char* err,
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

static QoreHashNode* qore_socket_exec_address_info(QoreSocket* s,
        QoreSocketControllerAddressInfoPollOperation::Action action, bool host_lookup, const char* owner_name,
        const char* err, ExceptionSink* xsink) {
    QoreSocketControllerAddressInfoPollOperation* poller = new QoreSocketControllerAddressInfoPollOperation(s,
        action, host_lookup);

    ValueHolder result(qore_socket_exec_poll(s,
        poller, -1, owner_name, "done", xsink), xsink);
    if (*xsink) {
        return nullptr;
    }

    return qore_socket_get_addr_info_from_output(*result, err, xsink);
}

static int qore_socket_exec_accept_replace(QoreSocket* s, SocketSource* source, int timeout_ms,
        ExceptionSink* xsink) {
    qore_socket_private* priv = qore_socket_private::get(*s);
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_ALL);
    if (!io_guard) {
        return -1;
    }

    QoreHashNode* ex = nullptr;
    ValueHolder result(qore_socket_exec_poll(s,
        new QoreSocketControllerAcceptReplacePollOperation(s, source),
        timeout_ms, "acceptAndReplace", "done", xsink, &ex), xsink);
    ReferenceHolder<QoreHashNode> ex_holder(ex, xsink);
    if (*xsink) {
        return -1;
    }
    if (ex_holder) {
        if (qore_socket_exec_exception_is(**ex_holder, "SOCKET-TIMEOUT")) {
            return QSE_TIMEOUT;
        }
        qore_socket_raise_poll_result_exception(*ex_holder, xsink);
        return -1;
    }
    if (result->isNothing()) {
        return -1;
    }
    return static_cast<int>(result->getAsBigInt());
}

int qore_socket_exec_close_private(qore_socket_private* priv) {
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

    ReferenceHolder<QoreSocketControllerPollable> pollable(
        new QoreSocketControllerPollable(priv, !qore_on_async_io_thread()), &xsink);
    int rc = ctrl->close(*pollable, &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

static int qore_socket_exec_close(QoreSocket* s) {
    return qore_socket_exec_close_private(qore_socket_private::get(*s));
}

static int qore_socket_exec_check_http1_allowed(QoreSocket* s, const char* mname, ExceptionSink* xsink) {
    ValueHolder rv(qore_socket_exec_poll(s, new QoreSocketControllerHttp1AllowedPollOperation(s, mname),
        -1, mname, "http1-checked", xsink), xsink);
    if (*xsink) {
        return -1;
    }
    return 0;
}

static int qore_socket_exec_send_http_message(QoreSocket* s, QoreHashNode* info,
        const char* method, const char* path, const char* http_version, const QoreHashNode* headers,
        const void* data, size_t size, const QoreStringNode* body_event, int source, int timeout_ms,
        ExceptionSink* xsink) {
    qore_socket_private* priv = qore_socket_private::get(*s);
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_SEND);
    if (!io_guard) {
        return -1;
    }

    if (qore_socket_exec_check_http1_allowed(s, "sendHTTPMessage", xsink)) {
        return -1;
    }

    QoreString hdr(s->getEncoding());
    priv->getSendHttpMessageHeaders(hdr, info, method, path, http_version, headers, size, source);

    SimpleRefHolder<BinaryNode> msg(new BinaryNode());
    msg->append(hdr.c_str(), hdr.size());
    if (size && data) {
        msg->append(data, size);
    }

    int rc = qore_socket_exec_send_bytes(s, msg->getPtr(), msg->size(), timeout_ms, xsink, -1,
        "sendHTTPMessage");
    if (!rc && size && data) {
        if (body_event) {
            priv->do_data_event(QORE_EVENT_SOCKET_DATA_SENT, source, *body_event);
        } else {
            priv->do_data_event(QORE_EVENT_SOCKET_DATA_SENT, source, data, size);
        }
    }
    return rc;
}

static int qore_socket_exec_send_http_response(QoreSocket* s, QoreHashNode* info,
        int code, const char* desc, const char* http_version, const QoreHashNode* headers,
        const void* data, size_t size, const QoreStringNode* body_event, int source, int timeout_ms,
        ExceptionSink* xsink) {
    SocketSyncPoll::assertNotOnIoThread("Socket", "sendHTTPResponse", xsink);
    if (*xsink) {
        return -1;
    }

    qore_socket_private* priv = qore_socket_private::get(*s);
    QoreSocketRawAsyncIoGuard send_guard(*priv, xsink, NB_SEND);
    if (!send_guard) {
        return -1;
    }

    int32_t stream_id = priv->getH2ActiveThreadStreamId();
    ValueHolder h2_stream(qore_socket_exec_poll(s,
        new QoreSocketControllerHttp2ServerStreamPollOperation(s, stream_id, "sendHTTPResponse"),
        -1, "sendHTTPResponse", "http2-server-stream-checked", xsink), xsink);
    if (*xsink) {
        return -1;
    }

    stream_id = static_cast<int32_t>(h2_stream->getAsBigInt());
    if (stream_id > 0) {
        QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_ALL);
        if (!io_guard) {
            return -1;
        }

        QoreString status_line(priv->enc);
        status_line.sprintf("HTTP/%s %03d %s", http_version, code, desc);
        if (info) {
            info->setKeyValue("response-uri", new QoreStringNode(status_line), nullptr);
        }

        ValueHolder rv(qore_socket_exec_poll(s,
            new QoreSocketControllerHttp2SendResponsePollOperation(s, stream_id, code, headers, data, size,
                body_event),
            timeout_ms, "sendHTTPResponse", "sent", xsink), xsink);
        return *xsink ? -1 : 0;
    }

    QoreString hdr(s->getEncoding());
    hdr.sprintf("HTTP/%s %03d %s", http_version, code, desc);
    if (info) {
        info->setKeyValue("response-uri", new QoreStringNode(hdr), nullptr);
    }
    priv->getSendHttpMessageHeadersCommon(hdr, info, headers, size, source);

    SimpleRefHolder<BinaryNode> msg(new BinaryNode());
    msg->append(hdr.c_str(), hdr.size());
    if (size && data) {
        msg->append(data, size);
    }

    int rc = qore_socket_exec_send_bytes(s, msg->getPtr(), msg->size(), timeout_ms, xsink, -1,
        "sendHTTPResponse");
    if (!rc && size && data) {
        if (body_event) {
            priv->do_data_event(QORE_EVENT_SOCKET_DATA_SENT, source, *body_event);
        } else {
            priv->do_data_event(QORE_EVENT_SOCKET_DATA_SENT, source, data, size);
        }
    }
    return rc;
}

static int qore_socket_exec_send_http_chunked_headers(QoreSocket* s, QoreHashNode* info,
        const char* method, const char* path, const char* http_version, const QoreHashNode* headers,
        int source, int timeout_ms, ExceptionSink* xsink) {
    if (qore_socket_exec_check_http1_allowed(s, "sendHTTPMessageWithCallback", xsink)) {
        return -1;
    }

    QoreString hdr(s->getEncoding());
    hdr.sprintf("%s %s HTTP/%s", method, path && path[0] ? path : "/", http_version);
    if (info) {
        info->setKeyValue("request-uri", new QoreStringNode(hdr), nullptr);
    }
    qore_socket_private::get(*s)->getSendHttpMessageHeadersCommon(hdr, info, headers, 0, source, false, true);
    return qore_socket_exec_send_bytes(s, hdr.c_str(), hdr.size(), timeout_ms, xsink, -1,
        "sendHTTPMessageWithCallback");
}

static void qore_socket_exec_set_http_chunk_prefix(QoreString& prefix, size_t size) {
    prefix.sprintf(QLLX "\r\n", static_cast<unsigned long long>(size));
}

static int qore_socket_exec_send_http_chunked_body_trailer(QoreSocket* s, const QoreHashNode* trailer,
        int source, int timeout_ms, ExceptionSink* xsink) {
    qore_socket_private* priv = qore_socket_private::get(*s);
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_SEND);
    if (!io_guard) {
        return -1;
    }

    QoreString hdr(s->getEncoding());
    hdr.concat("0\r\n");
    qore_socket_private::do_headers(hdr, trailer, 0, false);

    int rc = qore_socket_exec_send_bytes(s, hdr.c_str(), hdr.size(), timeout_ms, xsink, -1);
    if (!rc && trailer) {
        priv->do_header_event(QORE_EVENT_HTTP_FOOTERS_SENT, source, *trailer);
    }
    return rc;
}

static bool qore_socket_exec_check_send_aborted(QoreSocket* s, bool* aborted, ExceptionSink* xsink) {
    if (!aborted) {
        return false;
    }

    bool data_available = qore_socket_exec_is_data_available(s, 0, xsink);
    if (data_available || *xsink) {
        *aborted = true;
        return true;
    }
    return false;
}

static bool qore_socket_exec_try_clear_send_error_as_aborted(QoreSocket* s, bool* aborted, ExceptionSink* xsink) {
    if (!aborted || !*xsink) {
        return false;
    }

    ExceptionSink aborted_xsink;
    bool data_available = qore_socket_exec_is_data_available(s, 0, &aborted_xsink);
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

static int qore_socket_exec_send_http_chunked_body_callback(QoreSocket* s,
        const ResolvedCallReferenceNode* send_callback, int source, int timeout_ms, bool* aborted,
        ExceptionSink* xsink) {
    assert(!aborted || !(*aborted));

    unsigned cancel_check = 0;
    while (true) {
        if (!(cancel_check++ % 100) && qore_check_cancel(xsink, "socket chunked callback send")) {
            return -1;
        }

        if (qore_socket_exec_check_send_aborted(s, aborted, xsink)) {
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
                if (qore_socket_exec_send_http_chunked_body_trailer(s, res->get<const QoreHashNode>(), source,
                        timeout_ms, xsink)
                        && !qore_socket_exec_try_clear_send_error_as_aborted(s, aborted, xsink)) {
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
            if (qore_socket_exec_send_bytes(s, "0\r\n\r\n", 5, timeout_ms, xsink, -1)
                    && !qore_socket_exec_try_clear_send_error_as_aborted(s, aborted, xsink)) {
                return -1;
            }
            return 0;
        }

        QoreString prefix;
        qore_socket_exec_set_http_chunk_prefix(prefix, size);
        if (qore_socket_exec_send_bytes(s, prefix.c_str(), prefix.size(), timeout_ms, xsink, -1)
                || qore_socket_exec_send_bytes(s, data, size, timeout_ms, xsink, -1)
                || qore_socket_exec_send_bytes(s, "\r\n", 2, timeout_ms, xsink, -1)) {
            return qore_socket_exec_try_clear_send_error_as_aborted(s, aborted, xsink) ? 0 : -1;
        }

        if (body_event) {
            qore_socket_private::get(*s)->do_data_event(QORE_EVENT_HTTP_CHUNKED_DATA_SENT, source, *body_event);
        } else {
            qore_socket_private::get(*s)->do_data_event(QORE_EVENT_HTTP_CHUNKED_DATA_SENT, source, data, size);
        }
    }
}

static int qore_socket_exec_send_http_message_callback(QoreSocket* s, QoreHashNode* info,
        const char* method, const char* path, const char* http_version, const QoreHashNode* headers,
        const ResolvedCallReferenceNode* send_callback, int source, int timeout_ms, bool* aborted,
        ExceptionSink* xsink) {
    qore_socket_private* priv = qore_socket_private::get(*s);
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_SEND);
    if (!io_guard) {
        return -1;
    }

    if (qore_socket_exec_send_http_chunked_headers(s, info, method, path, http_version, headers, source,
            timeout_ms, xsink)) {
        return -1;
    }
    return qore_socket_exec_send_http_chunked_body_callback(s, send_callback, source, timeout_ms, aborted, xsink);
}

class QoreSocketControllerReadHttpHeaderPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketControllerReadHttpHeaderPollOperation(QoreSocket* sock, int source)
            : sock(sock), out(nullptr), source(source) {
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink*) override {
        bool close_socket = bytes_consumed;
        if (!close_socket && poll_state) {
            assert(dynamic_cast<SocketRecvUntilBytesPollState*>(poll_state.get()));
            close_socket = reinterpret_cast<SocketRecvUntilBytesPollState*>(poll_state.get())->getBytesReceived() > 0;
        }
        poll_state.reset();
        if (close_socket) {
            qore_socket_close_from_controller(sock);
        }
        out = nullptr;
        done = true;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (done) {
            return nullptr;
        }

        if (!poll_state) {
            poll_state.reset(sock->startRecvUntilBytes(xsink, "\r\n\r\n", 4));
            if (*xsink || !poll_state) {
                done = true;
                return nullptr;
            }
        }

        int rc = poll_state->continuePoll(xsink);
        if (*xsink) {
            poll_state.reset();
            done = true;
            return nullptr;
        }
        if (rc) {
            return getSocketPollInfoHash(xsink, rc);
        }

        SimpleRefHolder<BinaryNode> bin(poll_state->takeOutput().get<BinaryNode>());
        poll_state.reset();

        size_t raw_size = bin->size();
        bytes_consumed = raw_size > 0;

        out = new QoreHashNode(autoTypeInfo);
        out->setKeyValue("size", static_cast<int64>(raw_size), xsink);
        if (*xsink) {
            done = true;
            return nullptr;
        }
        if (!raw_size) {
            out->setKeyValue("closed", true, xsink);
            done = true;
            return nullptr;
        }

        char* buf = reinterpret_cast<char*>(bin->giveBuffer());
        char* nbuf = reinterpret_cast<char*>(q_realloc(buf, raw_size + 1));
        if (!nbuf) {
            xsink->outOfMemory();
            done = true;
            return nullptr;
        }
        nbuf[raw_size] = '\0';
        QoreStringNodeHolder hdrstr(new QoreStringNode(nbuf, raw_size, raw_size + 1, sock->getEncoding()));

        ReferenceHolder<QoreHashNode> info(new QoreHashNode(autoTypeInfo), xsink);
        ReferenceHolder<QoreHashNode> hdr(
            qore_socket_private::get(*sock)->processHttpHeaderString(xsink, hdrstr, *info, source), xsink);
        if (*xsink) {
            done = true;
            return nullptr;
        }

        out->setKeyValue("hdr", hdr.release(), xsink);
        out->setKeyValue("info", info.release(), xsink);
        done = true;
        return nullptr;
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return out.release();
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        return done ? "received" : "receiving";
    }

private:
    QoreSocket* sock;
    std::unique_ptr<AbstractPollState> poll_state;
    mutable ReferenceHolder<QoreHashNode> out;
    int source;
    bool bytes_consumed = false;
    bool done = false;
};

static QoreHashNode* qore_socket_exec_read_http_header(QoreSocket* s, QoreHashNode* info, int timeout_ms,
        qore_offset_t* rc, int source, ExceptionSink* xsink) {
    qore_socket_private* priv = qore_socket_private::get(*s);
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_RECV);
    if (!io_guard) {
        if (rc) {
            *rc = -1;
        }
        return nullptr;
    }

    ValueHolder result(qore_socket_exec_poll(s,
        new QoreSocketControllerReadHttpHeaderPollOperation(s, source),
        timeout_ms, "readHTTPHeader", "received", xsink), xsink);
    if (*xsink) {
        if (rc) {
            *rc = -1;
        }
        return nullptr;
    }
    if (result->getType() != NT_HASH) {
        xsink->raiseException("SOCKET-HTTP-ERROR",
            "expected hash output from async HTTP header operation, got '%s'", result->getFullTypeName());
        if (rc) {
            *rc = -1;
        }
        return nullptr;
    }

    QoreHashNode* output = result->get<QoreHashNode>();
    QoreValue size_val = output->getKeyValue("size");
    if (size_val.getType() != NT_INT) {
        xsink->raiseException("SOCKET-HTTP-ERROR", "missing raw HTTP header size from async header operation");
        if (rc) {
            *rc = -1;
        }
        return nullptr;
    }
    if (rc) {
        *rc = static_cast<qore_offset_t>(size_val.getAsBigInt());
    }

    if (output->getKeyValue("closed").getAsBool()) {
        xsink->raiseException("SOCKET-HTTP-ERROR", "remote closed the connection while reading the HTTP header");
        return nullptr;
    }

    QoreValue hdr_val = output->getKeyValue("hdr");
    if (hdr_val.getType() != NT_HASH) {
        xsink->raiseException("SOCKET-HTTP-ERROR",
            "expected 'hdr' hash output from async HTTP header operation, got '%s'", hdr_val.getFullTypeName());
        if (rc) {
            *rc = -1;
        }
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> hdr(hdr_val.get<const QoreHashNode>()->hashRefSelf(), xsink);
    if (info) {
        QoreValue info_val = output->getKeyValue("info");
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

static QoreStringNode* qore_socket_exec_read_http_header_string(QoreSocket* s, int timeout_ms, int source,
        ExceptionSink* xsink) {
    SimpleRefHolder<QoreStringNode> raw(qore_socket_exec_recv_until_string(s, "\r\n\r\n", 4, timeout_ms,
        "readHTTPHeaderString", xsink));
    if (*xsink) {
        return nullptr;
    }

    const size_t len = raw->size();
    SimpleRefHolder<QoreStringNode> hdr;
    if (len >= 4 && !memcmp(raw->c_str() + len - 4, "\r\n\r\n", 4)) {
        hdr = new QoreStringNode(raw->c_str(), len - 4, raw->getEncoding());
        hdr->concat('\n');
    } else {
        hdr = raw->stringRefSelf();
    }

    qore_socket_private::get(*s)->do_data_event(QORE_EVENT_HTTP_HEADERS_READ, source, **hdr);
    return hdr.release();
}

static size_t qore_socket_exec_http_line_payload_size(const QoreStringNode& line) {
    size_t len = line.size();
    while (len && (line.c_str()[len - 1] == '\r' || line.c_str()[len - 1] == '\n')) {
        --len;
    }
    return len;
}

static bool qore_socket_exec_http_blank_line(const QoreStringNode& line) {
    return !qore_socket_exec_http_line_payload_size(line);
}

static int qore_socket_exec_parse_http_chunk_size(const char* line_str, size_t line_len, int64_t& chunk_size,
        ExceptionSink* xsink) {
    const char* semi = static_cast<const char*>(memchr(line_str, ';', line_len));
    size_t hex_len = semi ? static_cast<size_t>(semi - line_str) : line_len;
    if (!hex_len) {
        xsink->raiseException("READ-HTTP-CHUNK-ERROR", "empty value given for chunk size");
        return -1;
    }

    std::string hex(line_str, hex_len);
    errno = 0;
    char* end = nullptr;
    unsigned long long parsed = strtoull(hex.c_str(), &end, 16);
    if (errno == ERANGE || end == hex.c_str() || *end) {
        xsink->raiseException("READ-HTTP-CHUNK-ERROR", "invalid value given for chunk size ('%s')", hex.c_str());
        return -1;
    }

    unsigned long long max_read_size = static_cast<unsigned long long>(
        std::numeric_limits<ssize_t>::max() - 2);
    if (parsed > max_read_size) {
        xsink->raiseException("READ-HTTP-CHUNK-ERROR", "chunk size %llu exceeds the maximum supported size",
            parsed);
        return -1;
    }

    chunk_size = static_cast<int64_t>(parsed);
    return 0;
}

class QoreSocketControllerReadHttpChunkedBodyPollOperation : public SocketPollOperationBase {
public:
    enum class State {
        RecvChunkSize,
        RecvChunkData,
        RecvTrailer,
        Done,
    };

    DLLLOCAL QoreSocketControllerReadHttpChunkedBodyPollOperation(QoreSocket* sock, bool binary_body,
            bool read_once, int source)
            : sock(sock), out(nullptr), body_bin(binary_body ? new BinaryNode : nullptr),
            body_str(binary_body ? nullptr : new QoreStringNode(sock->getEncoding())),
            trailers(sock->getEncoding()), binary_body(binary_body), read_once(read_once), source(source) {
        assert(!read_once || binary_body);
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink*) override {
        bool close_socket = abortNeedsClose();
        poll_state.reset();
        if (close_socket) {
            qore_socket_close_from_controller(sock);
        }
        out = nullptr;
        data.discard();
        state = State::Done;
        done = true;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (done) {
            return nullptr;
        }

        qore_socket_private* priv = qore_socket_private::get(*sock);
        if (!http_expect_cleared) {
            if (priv->http_exp_chunked_body) {
                priv->http_exp_chunked_body = false;
            }
            http_expect_cleared = true;
        }

        unsigned cancel_check = 0;
        while (!done) {
            if (!(cancel_check++ % 100) && qore_check_cancel(xsink, "socket chunked body read")) {
                state = State::Done;
                return nullptr;
            }

            if (!poll_state) {
                startPoll(xsink);
                if (*xsink || !poll_state) {
                    state = State::Done;
                    done = true;
                    return nullptr;
                }
            }

            int rc = poll_state->continuePoll(xsink);
            if (*xsink) {
                poll_state.reset();
                state = State::Done;
                return nullptr;
            }
            if (rc) {
                return getSocketPollInfoHash(xsink, rc);
            }

            if (takePollOutput(xsink)) {
                state = State::Done;
                return nullptr;
            }

            int handle_rc = 0;
            switch (state) {
                case State::RecvChunkSize:
                    handle_rc = handleChunkSize(xsink, *priv);
                    break;
                case State::RecvChunkData:
                    handle_rc = handleChunkData(xsink, *priv);
                    break;
                case State::RecvTrailer:
                    handle_rc = handleTrailer(xsink, *priv);
                    break;
                case State::Done:
                    done = true;
                    return nullptr;
            }
            if (handle_rc || *xsink) {
                state = State::Done;
                return nullptr;
            }
        }

        return nullptr;
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return out.release();
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        switch (state) {
            case State::RecvChunkSize:
                return "recv-chunk-size";
            case State::RecvChunkData:
                return "recv-chunk-data";
            case State::RecvTrailer:
                return "recv-chunk-trailer";
            case State::Done:
                return "received";
            default:
                return "unknown";
        }
    }

private:
    DLLLOCAL void startPoll(ExceptionSink* xsink) {
        switch (state) {
            case State::RecvChunkSize:
            case State::RecvTrailer:
                current_read_to_string = true;
                poll_state.reset(sock->startRecvUntilBytes(xsink, "\r\n", 2));
                break;
            case State::RecvChunkData:
                current_read_to_string = false;
                poll_state.reset(sock->startRecv(xsink, static_cast<size_t>(current_chunk_size + 2)));
                break;
            case State::Done:
                break;
        }
    }

    DLLLOCAL int takePollOutput(ExceptionSink* xsink) {
        SimpleRefHolder<BinaryNode> bin(poll_state->takeOutput().get<BinaryNode>());
        poll_state.reset();
        if (current_read_to_string) {
            size_t len = bin->size();
            char* buf = reinterpret_cast<char*>(bin->giveBuffer());
            char* nbuf = reinterpret_cast<char*>(q_realloc(buf, len + 1));
            if (!nbuf) {
                xsink->outOfMemory();
                return -1;
            }
            nbuf[len] = '\0';
            data = new QoreStringNode(nbuf, len, len + 1, sock->getEncoding());
        } else {
            data = bin.release();
        }
        return 0;
    }

    DLLLOCAL int setOutputBody(ExceptionSink* xsink) {
        out = new QoreHashNode(autoTypeInfo);
        if (binary_body) {
            out->setKeyValue("body", body_bin.release(), xsink);
        } else {
            out->setKeyValue("body", body_str.release(), xsink);
        }
        return *xsink ? -1 : 0;
    }

    DLLLOCAL int handleChunkSize(ExceptionSink* xsink, qore_socket_private& priv) {
        ValueHolder line_val(data ? data->refSelf() : QoreValue(), xsink);
        data.discard();
        if (*xsink) {
            return -1;
        }
        if (line_val->getType() != NT_STRING) {
            xsink->raiseException("READ-HTTP-CHUNK-ERROR",
                "expected string output from async chunk-size read operation, got '%s'",
                line_val->getFullTypeName());
            return -1;
        }

        const QoreStringNode* line = line_val->get<const QoreStringNode>();
        size_t line_len = qore_socket_exec_http_line_payload_size(*line);
        const char* line_str = line->c_str();
        int64_t chunk_size = 0;
        if (qore_socket_exec_parse_http_chunk_size(line_str, line_len, chunk_size, xsink)) {
            return -1;
        }

        priv.do_chunked_read(QORE_EVENT_HTTP_CHUNK_SIZE, static_cast<size_t>(chunk_size), line_len, source);
        bytes_consumed = true;

        if (!chunk_size) {
            if (setOutputBody(xsink)) {
                return -1;
            }
            state = State::RecvTrailer;
            return 0;
        }

        current_chunk_size = chunk_size;
        state = State::RecvChunkData;
        return 0;
    }

    DLLLOCAL int handleChunkData(ExceptionSink* xsink, qore_socket_private& priv) {
        ValueHolder data_val(data ? data->refSelf() : QoreValue(), xsink);
        data.discard();
        if (*xsink) {
            return -1;
        }
        if (data_val->getType() != NT_BINARY) {
            xsink->raiseException("READ-HTTP-CHUNK-ERROR",
                "expected binary output from async chunk data read operation, got '%s'",
                data_val->getFullTypeName());
            return -1;
        }

        const BinaryNode* bin = data_val->get<const BinaryNode>();
        size_t chunk_size = static_cast<size_t>(current_chunk_size);
        if (bin->size() < chunk_size + 2) {
            xsink->raiseException("READ-HTTP-CHUNK-ERROR", "read " QSD " bytes for HTTP chunk size " QSD,
                bin->size(), chunk_size);
            return -1;
        }

        priv.do_data_event(QORE_EVENT_HTTP_CHUNKED_DATA_READ, source, bin->getPtr(), chunk_size);
        if (binary_body) {
            body_bin->append(bin->getPtr(), chunk_size);
        } else {
            body_str->concat(static_cast<const char*>(bin->getPtr()), chunk_size);
        }
        bytes_consumed = true;

        int64 body_size = binary_body ? static_cast<int64>(body_bin->size()) : static_cast<int64>(body_str->size());
        int64 max_chunked_body_size = priv.max_chunked_body_size.load(std::memory_order_relaxed);
        if (max_chunked_body_size > 0 && body_size > max_chunked_body_size) {
            xsink->raiseException("HTTP-BODY-TOO-LARGE", "chunked body size " QLLD " exceeds maximum " QLLD,
                body_size, max_chunked_body_size);
            return -1;
        }

        priv.do_chunked_read(QORE_EVENT_HTTP_CHUNKED_DATA_RECEIVED, chunk_size, chunk_size + 2, source);
        if (read_once) {
            if (setOutputBody(xsink)) {
                return -1;
            }
            state = State::Done;
            done = true;
            return 0;
        }

        current_chunk_size = 0;
        state = State::RecvChunkSize;
        return 0;
    }

    DLLLOCAL int handleTrailer(ExceptionSink* xsink, qore_socket_private& priv) {
        ValueHolder line_val(data ? data->refSelf() : QoreValue(), xsink);
        data.discard();
        if (*xsink) {
            return -1;
        }
        if (line_val->getType() != NT_STRING) {
            xsink->raiseException("READ-HTTP-CHUNK-ERROR",
                "expected string output from async chunk trailer read operation, got '%s'",
                line_val->getFullTypeName());
            return -1;
        }

        const QoreStringNode* line = line_val->get<const QoreStringNode>();
        bytes_consumed = true;
        if (qore_socket_exec_http_blank_line(*line)) {
            if (!trailers.empty()) {
                priv.convertHeaderToHash(*out, const_cast<char*>(trailers.c_str()), 0, nullptr, nullptr,
                    "response-headers-raw");
                if (*xsink) {
                    return -1;
                }
                priv.do_read_http_header(QORE_EVENT_HTTP_FOOTERS_RECEIVED, *out, source);
            }
            state = State::Done;
            done = true;
            return 0;
        }

        trailers.concat(line->c_str(), line->size());
        return 0;
    }

    DLLLOCAL bool abortNeedsClose() const {
        if (bytes_consumed) {
            return true;
        }
        if (!poll_state) {
            return false;
        }
        if (current_read_to_string) {
            assert(dynamic_cast<SocketRecvUntilBytesPollState*>(poll_state.get()));
            return reinterpret_cast<SocketRecvUntilBytesPollState*>(poll_state.get())->getBytesReceived() > 0;
        }
        assert(dynamic_cast<SocketRecvPollState*>(poll_state.get()));
        return reinterpret_cast<SocketRecvPollState*>(poll_state.get())->getBytesReceived() > 0;
    }

    QoreSocket* sock;
    std::unique_ptr<AbstractPollState> poll_state;
    mutable ReferenceHolder<QoreHashNode> out;
    SimpleRefHolder<BinaryNode> body_bin;
    SimpleRefHolder<QoreStringNode> body_str;
    SimpleRefHolder<SimpleValueQoreNode> data;
    QoreString trailers;
    State state = State::RecvChunkSize;
    int64_t current_chunk_size = 0;
    bool binary_body = true;
    bool read_once = false;
    bool current_read_to_string = true;
    bool http_expect_cleared = false;
    int source = QORE_SOURCE_SOCKET;
    bool bytes_consumed = false;
    bool done = false;
};

static QoreHashNode* qore_socket_exec_read_http_chunked_body(QoreSocket* s, int timeout_ms,
        bool binary_body, bool read_once, const char* owner_name, int source, ExceptionSink* xsink) {
    SocketSyncPoll::assertNotOnIoThread("Socket", owner_name, xsink);
    if (*xsink) {
        return nullptr;
    }

    qore_socket_private* priv = qore_socket_private::get(*s);
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_RECV);
    if (!io_guard) {
        return nullptr;
    }

    ValueHolder rv(qore_socket_exec_poll(s,
        new QoreSocketControllerReadHttpChunkedBodyPollOperation(s, binary_body, read_once, source),
        timeout_ms, owner_name, "received", xsink), xsink);
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

static bool qore_socket_exec_process_sse_char(qore_socket_private* priv, QoreString& str, int& eol_count, char c) {
    if (priv->sse_got_cr) {
        priv->sse_got_cr = false;
        if (c == '\n') {
            return false;
        }
    }

    if (c == '\r') {
        str.concat('\n');
        priv->sse_got_cr = true;
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

static QoreHashNode* qore_socket_exec_read_server_sent_event(QoreSocket* s, int timeout_ms, ExceptionSink* xsink) {
    qore_socket_private* priv = qore_socket_private::get(*s);
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_RECV);
    if (!io_guard) {
        return nullptr;
    }

    ValueHolder rv(qore_socket_exec_poll(s, new QoreSocketControllerReadServerSentEventPollOperation(s),
        timeout_ms, "readServerSentEvent", "received", xsink), xsink);
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

static QoreHashNode* qore_socket_exec_read_server_sent_event_encoded(QoreSocket* s,
        const QoreStringNode* content_encoding, int timeout_ms, ExceptionSink* xsink) {
    qore_socket_private* priv = qore_socket_private::get(*s);
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_RECV);
    if (!io_guard) {
        return nullptr;
    }

    ValueHolder rv(qore_socket_exec_poll(s,
        new QoreSocketControllerReadServerSentEventPollOperation(s, content_encoding, xsink),
        timeout_ms, "readServerSentEvent", "received", xsink), xsink);
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

static int64 qore_socket_exec_recv_integer(QoreSocket* s, const char* meth, int len, void* targ, int timeout_ms,
        ExceptionSink* xsink) {
    SimpleRefHolder<BinaryNode> bin(qore_socket_exec_recv_binary(s, len, timeout_ms, xsink));
    if (*xsink) {
        return -1;
    }
    if (bin->size() != static_cast<size_t>(len)) {
        xsink->raiseException("SOCKET-RECV-ERROR",
            "expected %d byte(s) from Socket::%s(), got " QLLD,
            len, meth, static_cast<int64>(bin->size()));
        return -1;
    }
    memcpy(targ, bin->getPtr(), len);
    return len;
}

void se_ssl_already_established(const char* cname, const char* mname, ExceptionSink* xsink) {
    assert(xsink);
    xsink->raiseException("SOCKET-SSL-STATE-ERROR", "error in %s::%s(): SSL already established", cname, mname);
}

#ifdef _Q_WINDOWS
int sock_get_raw_error() {
    return WSAGetLastError();
}

static int qore_windows_set_errno(int rc) {
    switch (rc) {
        case 0:
            errno = 0;
            break;

        case WSANOTINITIALISED:
        case WSAEINVAL:
        case WSAENOTSOCK:
        case WSAEADDRNOTAVAIL:
        case WSAEAFNOSUPPORT:
        case WSAEOPNOTSUPP:
            errno = EINVAL;
            break;

        case WSAEADDRINUSE:
            errno = EIO;
            break;

        case WSAENETDOWN:
            errno = ENODEV;
            break;

        case WSAEFAULT:
            errno = EFAULT;
            break;

        case WSAENOBUFS:
            errno = ENOMEM;
            break;

        case WSAETIMEDOUT:
            errno = ETIMEDOUT;
            break;

        case WSAECONNREFUSED:
            errno = ECONNREFUSED;
            break;

        case WSAENOTCONN:
            errno = ENOTCONN;
            break;

        case WSAEBADF:
            errno = EBADF;
            break;

        case WSAECONNRESET:
        case WSAECONNABORTED:
            errno = ECONNRESET;
            break;

        case WSAEWOULDBLOCK:
        case WSAEALREADY:
        case WSAEINPROGRESS:
            errno = EAGAIN;
            break;

        case WSAEINTR:
            errno = EINTR;
            break;

        default:
            printd(0, "sock_get_error() unknown code %d; about to assert()\n", rc);
            assert(false);
            errno = EFAULT;
            break;
    }

    return errno;
}

int windows_set_errno() {
    return qore_windows_set_errno(WSAGetLastError());
}

int sock_get_error() {
    return windows_set_errno();
}

int check_windows_rc(int rc) {
    if (rc != SOCKET_ERROR) {
        return 0;
    }

    windows_set_errno();
    return -1;
}

void qore_socket_error_intern(int rc, ExceptionSink* xsink, const char* err, const char* cdesc, const char* mname,
        const char* host, const char* svc, const struct sockaddr *addr) {
    qore_windows_set_errno(rc);
    assert(xsink);

    QoreStringNode* desc = new QoreStringNode;
    if (mname)
        desc->sprintf("error while executing Socket::%s(): ", mname);

    desc->concat(cdesc);

    if (addr) {
        assert(!host);
        assert(!svc);

        concat_target(*desc, addr);
    } else {
        if (host && host[0]) {
            desc->sprintf(" (target: %s", host);
            if (svc && strcmp(svc, "-1")) {
                desc->sprintf(":%s", svc);
            }
            desc->concat(")");
        }
    }

    if (!errno) {
        xsink->raiseException(err, desc);
        return;
    }

    desc->concat(": ");
    char* buf;
    // get windows error message
    if (!FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER, 0, rc, LANG_USER_DEFAULT, (LPTSTR)&buf, 0, 0)) {
        assert(!buf);
        desc->sprintf("Windows FormatMessage() failed on error code %d", rc);
    } else
        assert(buf);

    desc->concat(buf);
    free(buf);

    xsink->raiseException(err, desc);
}

void qore_socket_error(ExceptionSink* xsink, const char* err, const char* cdesc, const char* mname, const char* host,
        const char* svc, const struct sockaddr *addr) {
    qore_socket_error_intern(WSAGetLastError(), xsink, err, cdesc, mname, host, svc, addr);
}
#else
int sock_get_raw_error() {
    return errno;
}

int sock_get_error() {
    return errno;
}

void qore_socket_error_intern(int rc, ExceptionSink* xsink, const char* err, const char* cdesc, const char* mname,
        const char* host, const char* svc, const struct sockaddr *addr) {
    assert(rc);
    assert(xsink);

    QoreStringNode* desc = new QoreStringNode;
    if (mname) {
        desc->sprintf("error while executing Socket::%s(): ", mname);
    }

    desc->concat(cdesc);

    if (addr) {
        assert(!host);
        assert(!svc);

        concat_target(*desc, addr);
    } else {
        if (host) {
            desc->sprintf(" (target: %s", host);
            if (svc && strcmp(svc, "-1")) {
                desc->sprintf(":%s", svc);
            }
            desc->concat(")");
        }
    }

    xsink->raiseErrnoException(err, rc, desc);
}

void qore_socket_error(ExceptionSink* xsink, const char* err, const char* cdesc, const char* mname, const char* host,
        const char* svc, const struct sockaddr *addr) {
    qore_socket_error_intern(errno, xsink, err, cdesc, mname, host, svc, addr);
}
#endif

int do_read_error(qore_offset_t rc, const char* method_name, int timeout_ms, ExceptionSink* xsink) {
    if (rc > 0) {
        return 0;
    }
    if (!*xsink) {
        QoreSocket::doException(rc, method_name, timeout_ms, xsink);
    }
    return -1;
}

void concat_target(QoreString& str, const struct sockaddr *addr, const char* type) {
    QoreString host;
    q_addr_to_string2(addr, host);
    if (!host.empty()) {
        int port = q_get_port_from_addr(addr);
        str.sprintf(" (%s: %s", type, host.c_str());
        if (port != -1) {
            str.sprintf(":%d", port);
        }
        str.concat(')');
    }
}

qore_socket_op_helper::qore_socket_op_helper(qore_socket_private* sock) : s(sock) {
    assert(s->in_op == -1);
    s->in_op = q_gettid();;
}

qore_socket_op_helper::~qore_socket_op_helper() {
    s->in_op = -1;
}

SSLSocketHelperHelper::SSLSocketHelperHelper(qore_socket_private* sock, bool set_thread_context) : s(sock) {
    assert(!s->ssl);
    ssl = s->ssl = new SSLSocketHelper(*sock);

    //printd(5, "SSLSocketHelperHelper::SSLSocketHelperHelper() priv: %p STC: %d CR: %d\n", s, set_thread_context, s->ssl_capture_remote_cert);

    if (set_thread_context && !qore_socket_private::current_socket && s->ssl_capture_remote_cert) {
        qore_socket_private::current_socket = s;
        context_saved = true;
    }
}

SSLSocketHelperHelper::~SSLSocketHelperHelper() {
    if (context_saved) {
        qore_socket_private::current_socket = nullptr;
        //printd(5, "SSLSocketHelperHelper::~SSLSocketHelperHelper() priv: %p RESET\n", s);
    }
}

void SSLSocketHelperHelper::error() {
    // s->ssl may already be nullptr if handleErrorIntern() closed the socket.
    // during SSL negotiation, which derefs ssl and nulls s->ssl; in that case the
    // SSLSocketReferenceHelper in setIntern() already deleted the object on deref
    if (s->ssl) {
        ssl->deref();
        s->ssl = nullptr;
    }
}

SSLSocketHelper::~SSLSocketHelper() {
    if (ssl) {
        SSL_free(ssl);
    }
    if (ctx) {
        SSL_CTX_free(ctx);
    }
}

int SSLSocketHelper::setIntern(ExceptionSink* xsink, const char* mname, int sd, QoreSSLCertificate* cert,
        QoreSSLPrivateKey* pkey) {
    SSLSocketReferenceHelper ssrh(this);

    assert(!ssl);
    assert(!ctx);
    ctx = SSL_CTX_new(meth);
    if (!ctx) {
        sslError(xsink, mname, "SSL_CTX_new");
        assert(*xsink);
        return -1;
    }
    if (cert) {
        if (!SSL_CTX_use_certificate(ctx, cert->getData())) {
            sslError(xsink, mname, "SSL_CTX_use_certificate");
            assert(*xsink);
            return -1;
        }
        if (!cert->priv->chain.empty()) {
            for (auto& i : cert->priv->chain) {
                if (!SSL_CTX_add_extra_chain_cert(ctx, X509_dup(i))) {
                    sslError(xsink, mname, "SSL_CTX_add_extra_chain_cert");
                    assert(*xsink);
                    return -1;
                }
            }
        }
    }
    if (pkey) {
        if (!SSL_CTX_use_PrivateKey(ctx, pkey->getData())) {
            sslError(xsink, mname, "SSL_CTX_use_PrivateKey");
            assert(*xsink);
            return -1;
        }
    }

    ssl = SSL_new(ctx);
    if (!ssl) {
        sslError(xsink, mname, "SSL_new");
        assert(*xsink);
        return -1;
    }

    SSL_set_ex_data(ssl, qore_ssl_data_index, &qs);

    // turn on SSL_MODE_ENABLE_PARTIAL_WRITE
    SSL_set_mode(ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);

    // turn on SSL_MODE_AUTO_RETRY for blocking I/O
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

    // Allow retrying SSL_write() with a moved buffer address (same contents).
    // Without this, if SSL_write() returns SSL_ERROR_WANT_WRITE and the send
    // buffer is reallocated before the retry (e.g. std::vector realloc in
    // Http2Session::sendPendingData), OpenSSL raises SSL_R_BAD_WRITE_RETRY.
    SSL_set_mode(ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

    // set the socket file descriptor
    SSL_set_fd(ssl, sd);

#ifdef SSL_OP_IGNORE_UNEXPECTED_EOF
    // treat EOF as a normal shutdown
    SSL_set_options(ssl, SSL_OP_IGNORE_UNEXPECTED_EOF);
#endif

    // set verification mode
    if (qs.ssl_verify_mode != SSL_VERIFY_NONE) {
        setVerifyMode(qs.ssl_verify_mode, qs.ssl_accept_all_certs, qs.client_target);
    }

#if defined(HAVE_SSL_SET_MAX_PROTO_VERSION) && defined(TLS1_3_VERSION)
    if (qore_library_options & QLO_MINIMUM_TLS_13) {
        if (!SSL_set_min_proto_version(ssl, TLS1_3_VERSION)) {
            sslError(xsink, mname, "SSL_set_min_proto_version");
            assert(*xsink);
            return -1;
        }
    } else if (qore_library_options & QLO_DISABLE_TLS_13) {
        if (!SSL_set_max_proto_version(ssl, TLS1_2_VERSION)) {
            sslError(xsink, mname, "SSL_set_max_proto_version");
            assert(*xsink);
            return -1;
        }
    }
#endif

    return 0;
}

int SSLSocketHelper::setClient(ExceptionSink* xsink, const char* mname, const char* sni_target_host, int sd,
        QoreSSLCertificate* cert, QoreSSLPrivateKey* pkey) {
#ifdef HAVE_TLS_SERVER_METHOD
    meth = TLS_client_method();
#else
    meth = SSLv23_client_method();
#endif
    int rc = setIntern(xsink, mname, sd, cert, pkey);
    if (!rc && sni_target_host) {
        // issue #3053 set TLS server name for servers that require SNI
        assert(ssl);
        SSLSocketReferenceHelper ssrh(this);
        ERR_clear_error();
        if (!SSL_set_tlsext_host_name(ssl, sni_target_host)) {
            sslError(xsink, mname, "SSL_set_tlsext_host_name");
            assert(*xsink);
            return -1;
        }
    }
    return rc;
}

int SSLSocketHelper::setServer(ExceptionSink* xsink, const char* mname, int sd, QoreSSLCertificate* cert,
        QoreSSLPrivateKey* pkey) {
#ifdef HAVE_TLS_SERVER_METHOD
    meth = TLS_server_method();
#else
    meth = SSLv23_server_method();
#endif
    return setIntern(xsink, mname, sd, cert, pkey);
}

int SSLSocketHelper::setAlpnProtocols(const std::vector<std::string>& protocols) {
    if (!ssl) {
        return -1;
    }

    // Build wire format: length-prefixed strings
    alpn_wire_format.clear();
    for (const auto& proto : protocols) {
        if (proto.empty() || proto.size() > 255) {
            continue;  // Skip invalid protocol names
        }
        alpn_wire_format.push_back(static_cast<unsigned char>(proto.size()));
        alpn_wire_format.insert(alpn_wire_format.end(), proto.begin(), proto.end());
    }

    if (alpn_wire_format.empty()) {
        return -1;
    }

    // Set ALPN protocols for client
    ERR_clear_error();
    if (SSL_set_alpn_protos(ssl, alpn_wire_format.data(), alpn_wire_format.size()) != 0) {
        return -1;
    }
    return 0;
}

void SSLSocketHelper::setServerAlpnProtocols(const std::vector<std::string>& protocols) {
    server_alpn_protocols = protocols;

    if (ctx && !protocols.empty()) {
        // Set the ALPN selection callback for server
        SSL_CTX_set_alpn_select_cb(ctx, alpnSelectCallback, this);
    }
}

int SSLSocketHelper::alpnSelectCallback(SSL* ssl, const unsigned char** out, unsigned char* outlen,
        const unsigned char* in, unsigned int inlen, void* arg) {
    SSLSocketHelper* helper = static_cast<SSLSocketHelper*>(arg);
    if (!helper || helper->server_alpn_protocols.empty()) {
        return SSL_TLSEXT_ERR_NOACK;
    }

    // Parse client protocols and select first match
    const unsigned char* p = in;
    const unsigned char* end = in + inlen;

    while (p < end) {
        unsigned char proto_len = *p++;
        if (p + proto_len > end) {
            break;
        }

        std::string client_proto(reinterpret_cast<const char*>(p), proto_len);
        for (const auto& supported : helper->server_alpn_protocols) {
            if (client_proto == supported) {
                *out = p;
                *outlen = proto_len;
                return SSL_TLSEXT_ERR_OK;
            }
        }
        p += proto_len;
    }

    return SSL_TLSEXT_ERR_NOACK;
}

std::string SSLSocketHelper::getAlpnProtocol() const {
    if (!ssl) {
        return "";
    }

    const unsigned char* proto = nullptr;
    unsigned int len = 0;
    SSL_get0_alpn_selected(ssl, &proto, &len);
    if (proto && len > 0) {
        return std::string(reinterpret_cast<const char*>(proto), len);
    }
    return "";
}

bool SSLSocketHelper::isHttp2() const {
    return getAlpnProtocol() == "h2";
}

int SSLSocketHelper::pending() const {
    return ssl ? SSL_pending(ssl) : 0;
}

// returns 0 = success, 1 = need SOCK_POLLIN, 2 = need SOCK_POLLOUT, < 0 = error
int SSLSocketHelper::startConnect(ExceptionSink* xsink) {
    SSLSocketReferenceHelper ssrh(this, true);

    OptionalNonBlockingHelper nbh(qs, true, xsink);
    if (*xsink) {
        return QSE_SSL_ERR;
    }

    ERR_clear_error();
    int rc = SSL_connect(ssl);

    if (rc != 1) {
        int err = SSL_get_error(ssl, rc);
        //printd(5, "SSLSocketHelper::startConnect() rc: %d err: %d\n", rc, err);
        switch (err) {
            case SSL_ERROR_WANT_READ:
                return SOCK_POLLIN;
            case SSL_ERROR_WANT_WRITE:
                return SOCK_POLLOUT;
            case SSL_ERROR_SYSCALL: {
                return sysCallError(xsink, rc, "startConnect", "SSL_connect");
            }
            case SSL_ERROR_ZERO_RETURN:
                // remote closed the connection
                break;
            default:
                printd(0, "SSLSocketHelper::startConnect() SSL_get_error() reports error %d\n", err);
                break;
        }

        if (sslError(xsink, "startConnect", "SSL_connect", true)) {
            return QSE_SSL_ERR;
        }
    }

    return 0;
}

// returns 0 for success, 1 = need SOCK_POLLIN, 2 = need SOCK_POLLOUT, < 0 = error
int SSLSocketHelper::startAccept(ExceptionSink* xsink) {
    SSLSocketReferenceHelper ssrh(this, true);

    OptionalNonBlockingHelper nbh(qs, true, xsink);
    if (*xsink) {
        return QSE_SSL_ERR;
    }

    ERR_clear_error();
    int rc = SSL_accept(ssl);

    if (rc != 1) {
        int err = SSL_get_error(ssl, rc);
        switch (err) {
            case SSL_ERROR_WANT_READ:
                return SOCK_POLLIN;
            case SSL_ERROR_WANT_WRITE:
                return SOCK_POLLOUT;
            case SSL_ERROR_SYSCALL: {
                return sysCallError(xsink, rc, "startAccept", "SSL_accept");
            }
            default:
                printd(3, "SSLSocketHelper::startAccept() err: %d\n", err);
                break;
        }
        if (sslError(xsink, "startAccept", "SSL_accept", true)) {
            return QSE_SSL_ERR;
        }
    }

    return 0;
}

int SSLSocketHelper::startShutdown(ExceptionSink* xsink) {
    SSLSocketReferenceHelper ssrh(this, true);

    OptionalNonBlockingHelper nbh(qs, true, xsink);
    if (*xsink) {
        return QSE_SSL_ERR;
    }

    ERR_clear_error();
    int rc = SSL_shutdown(ssl);
    if (rc < 0) {
        int err = SSL_get_error(ssl, rc);
        switch (err) {
            case SSL_ERROR_WANT_READ:
                return SOCK_POLLIN;
            case SSL_ERROR_WANT_WRITE:
                return SOCK_POLLOUT;
            case SSL_ERROR_SYSCALL:
                return sysCallError(xsink, rc, "shutdownSSL", "SSL_shutdown");
            default:
                break;
        }
        if (sslError(xsink, "shutdownSSL", "SSL_shutdown", true)) {
            return QSE_SSL_ERR;
        }
    }

    return 0;
}

int SSLSocketHelper::sysCallError(ExceptionSink* xsink, int rc, const char* mname, const char* ssl_func) {
     if (!sslError(xsink, mname, ssl_func)) {
        if (!rc) {
            xsink->raiseException("SOCKET-SSL-ERROR", "error in Socket::%s(): the openssl library reported an " \
                "EOF condition that violates the SSL protocol while calling %s()", mname, ssl_func);
        } else if (rc == -1) {
            xsink->raiseErrnoException("SOCKET-SSL-ERROR", sock_get_error(), "error in Socket::%s(): the " \
                "openssl library reported an I/O error while calling %s()", mname, ssl_func);

#ifdef ECONNRESET
            // close the socket if connection reset received
            // do not access "this" after the connection is closed since the SSLSocketHelper has been deleted
            if (qs.isOpen() && sock_get_error() == ECONNRESET)
                qore_socket_close_private_from_controller(&qs);
#endif
        } else {
            xsink->raiseException("SOCKET-SSL-ERROR", "error in Socket::%s(): the openssl library reported " \
                "error code %d in %s() but the error queue is empty", mname, rc, ssl_func);
        }
    }

    assert(*xsink);
    return QSE_SSL_ERR;
}

// returns 0 for success
int SSLSocketHelper::shutdown() {
   if (SSL_shutdown(ssl) < 0)
      return -1;
   return 0;
}

// returns 0 for success
int SSLSocketHelper::shutdown(ExceptionSink* xsink) {
    ERR_clear_error();
    if (SSL_shutdown(ssl) < 0) {
        SSLSocketReferenceHelper ssrh(this);
        sslError(xsink, "shutdownSSL", "SSL_shutdown");
        return -1;
    }
    return 0;
}

const char* SSLSocketHelper::getCipherName() const {
    return SSL_get_cipher_name(ssl);
}

const char* SSLSocketHelper::getCipherVersion() const {
    return SSL_get_cipher_version(ssl);
}

X509* SSLSocketHelper::getPeerCertificate() const {
    return SSL_get_peer_certificate(ssl);
}

long SSLSocketHelper::verifyPeerCertificate() const {
    X509* cert = SSL_get_peer_certificate(ssl);

    if (!cert) {
        return -1;
    }

    long rc = SSL_get_verify_result(ssl);
    X509_free(cert);
    return rc;
}

my_socket_priv::my_socket_priv(QoreSocket* s, QoreSSLCertificate* c, QoreSSLPrivateKey* p)
        : socket(s), cert(c), pk(p) {
    // Wire the back-pointer so sync I/O helpers can release this
    // mutex during their poll-wait phase.  See qore_socket_private::outer_lock.
    socket->priv->outer_lock = &m;
}

my_socket_priv::my_socket_priv() : socket(new QoreSocket) {
    socket->priv->outer_lock = &m;
}

void my_socket_priv::doDataEvent(int event, int source, const QoreStringNode& str) const {
    socket->priv->do_data_event(event, source, str);
}

void my_socket_priv::doDataEvent(int event, int source, const void* data, size_t size) const {
    socket->priv->do_data_event(event, source, data, size);
}

void my_socket_priv::doHeaderEvent(int event, int source, const QoreHashNode& hdr) const {
    socket->priv->do_header_event(event, source, hdr);
}

void my_socket_priv::doChunkedReadEvent(int event, size_t bytes, size_t total_read, int source) const {
    socket->priv->do_chunked_read(event, bytes, total_read, source);
}

void my_socket_priv::doReadHttpHeaderEvent(int event, const QoreHashNode& hdr, int source) const {
    socket->priv->do_read_http_header(event, &hdr, source);
}

void my_socket_priv::convertHeaderToHash(QoreHashNode& h, QoreString& hdr, QoreHashNode* info) const {
    socket->priv->convertHeaderToHash(&h, const_cast<char*>(hdr.c_str()), 0, info, nullptr,
        "response-headers-raw");
}

void my_socket_priv::clearHttpExpectChunkedBody() const {
    if (socket->priv->http_exp_chunked_body) {
        socket->priv->http_exp_chunked_body = false;
    }
}

int64 my_socket_priv::getMaxChunkedBodySize() const {
    return socket->priv->max_chunked_body_size.load(std::memory_order_relaxed);
}

bool my_socket_priv::takeSseGotCr() const {
    AutoLocker al(m);
    bool got_cr = socket->priv->sse_got_cr;
    if (got_cr) {
        socket->priv->sse_got_cr = false;
    }
    return got_cr;
}

void my_socket_priv::setSseGotCr(bool got_cr) const {
    AutoLocker al(m);
    socket->priv->sse_got_cr = got_cr;
}

int my_socket_priv::getSendHttpMessageHeaders(ExceptionSink* xsink, QoreString& hdr, QoreHashNode* info,
        const char* method, const char* path, const char* http_version, const QoreHashNode* headers,
        size_t size, int source) const {
    socket->priv->getSendHttpMessageHeaders(hdr, info, method, path, http_version, headers, size, source);
    return 0;
}

int my_socket_priv::getSendHttpMessageChunkedHeaders(ExceptionSink* xsink, QoreString& hdr, QoreHashNode* info,
        const char* method, const char* path, const char* http_version, const QoreHashNode* headers,
        int source) const {
    hdr.sprintf("%s %s HTTP/%s", method, path && path[0] ? path : "/", http_version);
    if (info) {
        info->setKeyValue("request-uri", new QoreStringNode(hdr), nullptr);
    }
    socket->priv->getSendHttpMessageHeadersCommon(hdr, info, headers, 0, source, false, true);
    return 0;
}

int32_t my_socket_priv::getH2ActiveServerStreamId() const {
    return socket->priv->getH2ActiveThreadStreamId();
}

int32_t my_socket_priv::getH2ActiveThreadStreamId() const {
    return socket->priv->getH2ActiveThreadStreamId();
}

bool my_socket_priv::hasH2SessionForAsyncPoll() const {
    return static_cast<bool>(socket->priv->h2_session);
}

bool my_socket_priv::isH2ServerSessionForAsyncPoll() const {
    return socket->priv->h2_session && socket->priv->h2_session->isServer();
}

void my_socket_priv::getSendHttpResponseStatusLine(QoreString& hdr, QoreHashNode* info, int code, const char* desc,
        const char* http_version) const {
    hdr.sprintf("HTTP/%s %03d %s", http_version, code, desc);
    if (info) {
        info->setKeyValue("response-uri", new QoreStringNode(hdr), nullptr);
    }
}

int my_socket_priv::getSendHttpResponseHeaders(ExceptionSink* xsink, QoreString& hdr, QoreHashNode* info, int code,
        const char* desc, const char* http_version, const QoreHashNode* headers, size_t size, int source) const {
    getSendHttpResponseStatusLine(hdr, info, code, desc, http_version);
    socket->priv->getSendHttpMessageHeadersCommon(hdr, info, headers, size, source);
    return 0;
}

int my_socket_priv::getSendHttpResponseChunkedHeaders(ExceptionSink* xsink, QoreString& hdr, QoreHashNode* info,
        int code, const char* desc, const char* http_version, const QoreHashNode* headers, int source) const {
    getSendHttpResponseStatusLine(hdr, info, code, desc, http_version);

    socket->priv->getSendHttpMessageHeadersCommon(hdr, info, headers, 0, source, false, true);
    return 0;
}

int my_socket_priv::checkOpen(ExceptionSink* xsink) {
    // must be called with the lock held
    assert(m.trylock());

    if (checkValid(xsink)) {
        return -1;
    }

    if (!socket->priv->isOpen()) {
        xsink->raiseException("SOCKET-NOT-OPEN", "the underlying socket object is not open, so the operation "
            "cannot continue");
        return -1;
    }

    return 0;
}

int my_socket_priv::checkOpenAndNotSsl(ExceptionSink* xsink) {
    if (checkOpen(xsink)) {
        return -1;
    }

    if (socket->priv->ssl) {
        xsink->raiseException("SOCKET-SSL-CONNECTED", "a TLS/SSL connection has already been established");
        return -1;
    }

    return 0;
}

thread_local qore_socket_private* qore_socket_private::current_socket;

static int q_ssl_verify_accept_all(int preverify_ok, X509_STORE_CTX* x509_ctx) {
    //printd(5, "q_ssl_verify_accept_all() preverify_ok: %d x509_ctx: %p\n", preverify_ok, x509_ctx);
    // issue #3512: get remote certificate if applicable
    qore_socket_private::captureRemoteCert(x509_ctx);
    // accept all certificates
    return 1;
}

static int q_ssl_verify_accept_default(int preverify_ok, X509_STORE_CTX* x509_ctx) {
    printd(5, "q_ssl_verify_accept_default() preverify_ok: %d x509_ctx: %p\n", preverify_ok, x509_ctx);

    // issue #3512: get remote certificate if applicable
    qore_socket_private::captureRemoteCert(x509_ctx);

    // issue #3818: get verbose info for SSL error
    if (!preverify_ok) {
        SSL* ssl = reinterpret_cast<SSL*>(X509_STORE_CTX_get_ex_data(x509_ctx, SSL_get_ex_data_X509_STORE_CTX_idx()));
        qore_socket_private* qs = reinterpret_cast<qore_socket_private*>(SSL_get_ex_data(ssl, qore_ssl_data_index));

        X509* err_cert = X509_STORE_CTX_get_current_cert(x509_ctx);
        int err = X509_STORE_CTX_get_error(x509_ctx);
        int depth = X509_STORE_CTX_get_error_depth(x509_ctx);

        char buf[256];
        X509_NAME_oneline(X509_get_subject_name(err_cert), buf, 256);

        SimpleRefHolder<QoreStringNode> ssl_err(new QoreStringNodeMaker("verify error %d depth %d: %s: %s", err,
            depth, X509_verify_cert_error_string(err), buf));

        // At this point, err contains the last verification error
        if (err == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT) {
            X509_NAME_oneline(X509_get_issuer_name(err_cert), buf, 256);
            ssl_err->sprintf(", issuer: %s", buf);
        }

        qs->setSslErrorString(ssl_err.release());
    }

    return preverify_ok;
}

void SSLSocketHelper::setVerifyMode(int mode, bool accept_all_certs, const std::string& target) {
    printd(5, "SSLSocketHelper::setVerifyMode() mode: %d accept_all_certs: %d target: %s\n", mode,
        (int)accept_all_certs, target.c_str());
    if (!accept_all_certs) {
        // issue #3818: load default CAs
        SSL_CTX_set_default_verify_paths(ctx);

#if defined(HAVE_SSL_SET_HOSTFLAGS) && defined(HAVE_SSL_SET1_HOST)
        // issue #3808: enable hostname validation with certificate validation, otherwise all valid certificates are
        // accepted, even if the hostname does not match; see:
        // https://gist.github.com/theopolis/aeaa8e4808f6b09328dd6996a2ed6c34
        SSL_set_hostflags(ssl, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
        if (!SSL_set1_host(ssl, target.c_str())) {
            // FIXME: openssl docs do not specify what can cause the SSL_set1_host() call to fail
            printd(0, "DEBUG: SSL_set1_host() %s failed\n", target.c_str());
        }
#endif
    }

    SSL_set_verify(ssl, mode, accept_all_certs ? q_ssl_verify_accept_all : q_ssl_verify_accept_default);
}

bool SSLSocketHelper::captureRemoteCert() const {
    if (!qore_socket_private::current_socket && qs.ssl_capture_remote_cert) {
        qore_socket_private::current_socket = &qs;
        //printd(5, "SSLSocketHelper::captureRemoteCert() priv: %p current_sock: %p\n", &qs, &qs);
        return true;
    }
    //printd(5, "SSLSocketHelper::captureRemoteCert() priv: %p FALSE\n", &qs);
    return false;
}

void SSLSocketHelper::clearRemoteCertContext() const {
    assert(qore_socket_private::current_socket == &qs);
    qore_socket_private::current_socket = nullptr;
    //printd(5, "SSLSocketHelper::clearRemoteCertContext()\n");
}

SSLSocketReferenceHelper::SSLSocketReferenceHelper(SSLSocketHelper* s, bool set_thread_context) : s(s) {
    s->ref();
    if (set_thread_context && s->captureRemoteCert()) {
        context_saved = true;
    }
}

SSLSocketReferenceHelper::~SSLSocketReferenceHelper() {
    if (context_saved) {
        s->clearRemoteCertContext();
    }
    s->deref();
}

SocketSource::SocketSource() : priv(new qore_socketsource_private) {
}

SocketSource::~SocketSource() {
   delete priv;
}

QoreStringNode* SocketSource::takeAddress() {
   QoreStringNode* addr = priv->address;
   priv->address = 0;
   return addr;
}

QoreStringNode* SocketSource::takeHostName() {
   QoreStringNode* host = priv->hostname;
   priv->hostname = 0;
   return host;
}

const char* SocketSource::getAddress() const {
   return priv->address ? priv->address->c_str() : 0;
}

const char* SocketSource::getHostName() const {
   return priv->hostname ? priv->hostname->c_str() : 0;
}

void SocketSource::setAll(QoreObject *o, ExceptionSink* xsink) {
   return priv->setAll(o, xsink);
}

static int qore_cares_library_init(ExceptionSink* xsink) {
    static std::once_flag init_once;
    static int init_status = ARES_SUCCESS;
    std::call_once(init_once, []() {
        init_status = ares_library_init(ARES_LIB_INIT_ALL);
    });
    if (init_status != ARES_SUCCESS) {
        xsink->raiseException("QOREADDRINFO-GETINFO-ERROR", "c-ares initialization failed: %s",
            ares_strerror(init_status));
        return -1;
    }
    return 0;
}

QoreCaresAddrInfoResolver::QoreCaresAddrInfoResolver(std::string host, std::string service, int family, int type,
        int protocol, int flags)
        : host(std::move(host)), service(std::move(service)), has_host(true), has_service(true), family(family),
        type(type), protocol(protocol), flags(flags) {
}

QoreCaresAddrInfoResolver::QoreCaresAddrInfoResolver(const char* host, const char* service, int family, int type,
        int protocol, int flags)
        : host(host ? host : ""), service(service ? service : ""), has_host(host), has_service(service),
        family(family), type(type), protocol(protocol), flags(flags) {
    if (!host && (flags & AI_PASSIVE)) {
        this->host = family == AF_INET6 ? "::" : "0.0.0.0";
        has_host = true;
    }
}

QoreCaresAddrInfoResolver::~QoreCaresAddrInfoResolver() {
    if (result) {
        ares_freeaddrinfo(result);
        result = nullptr;
    }
    if (channel) {
        ares_destroy(channel);
        channel = nullptr;
    }
}

int QoreCaresAddrInfoResolver::continuePoll(ExceptionSink* xsink) {
    if (!started && start(xsink)) {
        return -1;
    }
    if (done) {
        return status == ARES_SUCCESS ? 0 : -1;
    }

    process(xsink);
    if (*xsink) {
        return -1;
    }
    return done ? (status == ARES_SUCCESS ? 0 : -1) : 1;
}

void QoreCaresAddrInfoResolver::getExtraFds(std::vector<std::pair<int, int>>& fds) const {
    if (!channel || done) {
        return;
    }

    for (auto& [fd, events] : fd_events) {
        fds.push_back({fd, events});
    }
}

int QoreCaresAddrInfoResolver::getPollTimeoutMs() const {
    if (!channel || done) {
        return -1;
    }

    struct timeval tv;
    struct timeval* tvp = ares_timeout(channel, nullptr, &tv);
    if (!tvp) {
        return -1;
    }
    int64_t ms = static_cast<int64_t>(tvp->tv_sec) * 1000 + (tvp->tv_usec + 999) / 1000;
    return ms <= 0 ? 1 : static_cast<int>(std::min<int64_t>(ms, INT_MAX));
}

int QoreCaresAddrInfoResolver::start(ExceptionSink* xsink) {
    if (qore_cares_library_init(xsink)) {
        return -1;
    }

    struct ares_options options = {};
    options.sock_state_cb = sockStateCallback;
    options.sock_state_cb_data = this;

    int rc = ares_init_options(&channel, &options, ARES_OPT_SOCK_STATE_CB);
    if (rc != ARES_SUCCESS) {
        xsink->raiseException("QOREADDRINFO-GETINFO-ERROR", "ares_init_options() failed: %s",
            ares_strerror(rc));
        return -1;
    }

    struct ares_addrinfo_hints hints = {};
    hints.ai_flags = flags;
    hints.ai_family = family;
    hints.ai_socktype = type;
    hints.ai_protocol = protocol;

    started = true;
    ares_getaddrinfo(channel, has_host ? host.c_str() : nullptr, has_service ? service.c_str() : nullptr,
        &hints, callback, this);
    if (done) {
        return status == ARES_SUCCESS ? 0 : raiseError(xsink);
    }
    return 0;
}

void QoreCaresAddrInfoResolver::process(ExceptionSink* xsink) {
    std::vector<std::pair<int, int>> fds;
    getExtraFds(fds);
    if (fds.empty()) {
        ares_process_fd(channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
    } else {
        for (auto& [fd, events] : fds) {
            ares_socket_t rfd = (events & SOCK_POLLIN) ? fd : ARES_SOCKET_BAD;
            ares_socket_t wfd = (events & SOCK_POLLOUT) ? fd : ARES_SOCKET_BAD;
            ares_process_fd(channel, rfd, wfd);
            if (done) {
                break;
            }
        }
    }
    if (done && status != ARES_SUCCESS) {
        raiseError(xsink);
    }
}

void QoreCaresAddrInfoResolver::callback(void* arg, int status, int, struct ares_addrinfo* res) {
    reinterpret_cast<QoreCaresAddrInfoResolver*>(arg)->complete(status, res);
}

void QoreCaresAddrInfoResolver::sockStateCallback(void* arg, ares_socket_t socket_fd, int readable, int writable) {
    reinterpret_cast<QoreCaresAddrInfoResolver*>(arg)->updateFd(socket_fd, readable, writable);
}

void QoreCaresAddrInfoResolver::updateFd(ares_socket_t socket_fd, int readable, int writable) {
    int events = 0;
    if (readable) {
        events |= SOCK_POLLIN;
    }
    if (writable) {
        events |= SOCK_POLLOUT;
    }
    int fd = static_cast<int>(socket_fd);
    if (events) {
        fd_events[fd] = events;
    } else {
        fd_events.erase(fd);
    }
}

void QoreCaresAddrInfoResolver::complete(int new_status, struct ares_addrinfo* res) {
    status = new_status;
    result = res;
    if (status == ARES_SUCCESS && result) {
        bool first = true;
        const char* canonname = nullptr;
        if (flags & AI_CANONNAME) {
            canonname = result->name && *result->name
                ? result->name
                : (result->cnames && result->cnames->name && *result->cnames->name ? result->cnames->name
                    : nullptr);
        }
        for (struct ares_addrinfo_node* n = result->nodes; n; n = n->ai_next) {
            if (!n->ai_addr || n->ai_addrlen <= 0
                    || n->ai_addrlen > static_cast<ares_socklen_t>(sizeof(struct sockaddr_storage))) {
                continue;
            }
            SocketResolvedAddrInfo ai;
            ai.family = n->ai_family;
            ai.socktype = n->ai_socktype;
            ai.protocol = n->ai_protocol;
            ai.addrlen = static_cast<socklen_t>(n->ai_addrlen);
            memcpy(&ai.addr, n->ai_addr, n->ai_addrlen);
            if (first && canonname) {
                ai.canonname = canonname;
            }
            addrs.push_back(ai);
            first = false;
        }
        if (addrs.empty()) {
            status = ARES_ENODATA;
        }
    }
    done = true;
}

int QoreCaresAddrInfoResolver::raiseError(ExceptionSink* xsink) const {
    xsink->raiseException("QOREADDRINFO-GETINFO-ERROR",
        "ares_getaddrinfo(node: '%s', service: '%s', address_family: %d='%s') error: %s",
        has_host ? host.c_str() : "", has_service ? service.c_str() : "", family, q_af_to_str(family),
        ares_strerror(status));
    return -1;
}

static QoreListNode* qore_socket_resolved_addrinfo_to_list(const std::vector<SocketResolvedAddrInfo>& addrs,
        bool has_service) {
    QoreListNode* l = new QoreListNode(autoHashTypeInfo);

    for (const SocketResolvedAddrInfo& ai : addrs) {
        const struct sockaddr* addr = reinterpret_cast<const struct sockaddr*>(&ai.addr);
        QoreHashNode* h = new QoreHashNode(autoTypeInfo);
        qore_hash_private* hh = qore_hash_private::get(*h);

        if (!ai.canonname.empty()) {
            hh->setKeyValueIntern("canonname", new QoreStringNode(ai.canonname));
        }

        QoreStringNode* addr_str = q_addr_to_string2(addr);
        if (addr_str) {
            hh->setKeyValueIntern("address", addr_str);
            hh->setKeyValueIntern("address_desc", QoreAddrInfo::getAddressDesc(ai.family, addr_str->getBuffer()));
        }

        hh->setKeyValueIntern("family", ai.family);
        hh->setKeyValueIntern("familystr", new QoreStringNode(QoreAddrInfo::getFamilyName(ai.family)));
        hh->setKeyValueIntern("socktype", ai.socktype);
        hh->setKeyValueIntern("protocol", ai.protocol);
        hh->setKeyValueIntern("addrlen", static_cast<int64>(ai.addrlen));
        if (has_service) {
            int port = q_get_port_from_addr(addr);
            if (port != -1) {
                hh->setKeyValueIntern("port", port);
            }
        }

        l->push(h, nullptr);
    }

    return l;
}

class QoreSocketControllerResolveAddrInfoPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketControllerResolveAddrInfoPollOperation(const char* node, const char* service, int family,
            int socktype, int protocol, int flags, AbstractAsyncAction* action = nullptr)
            : node(node ? node : ""), service(service ? service : ""), has_node(node), has_service(service),
            family(q_get_af(family)), socktype(q_get_sock_type(socktype)), protocol(protocol), flags(flags),
            action(action) {
    }

    DLLLOCAL virtual ~QoreSocketControllerResolveAddrInfoPollOperation() {
        ExceptionSink xsink;
        cleanupAction(&xsink);
        if (xsink) {
            xsink.clear();
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink*) override {
        done = true;
        resolver.reset();
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (done) {
            return nullptr;
        }

        if (!resolver) {
            resolver = std::make_unique<QoreCaresAddrInfoResolver>(
                has_node ? node.c_str() : nullptr,
                has_service ? service.c_str() : nullptr,
                family, socktype, protocol, flags);
        }

        int rc = resolver->continuePoll(xsink);
        if (*xsink || rc < 0) {
            done = true;
            resolver.reset();
            return nullptr;
        }
        if (rc) {
            return getResolverPollInfo(xsink);
        }

        addrs = resolver->getAddresses();
        resolver.reset();
        done = true;
        return nullptr;
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return done ? qore_socket_resolved_addrinfo_to_list(addrs, has_service) : QoreValue();
    }

    DLLLOCAL virtual bool handleCompletion(bool canceled, const QoreHashNode* ex_hash,
            ExceptionSink* xsink) override {
        if (!action) {
            return false;
        }

        if (canceled) {
            action->executeError("QOREADDRINFO-CANCELLED", "async address resolution was canceled", xsink);
            cleanupAction(xsink);
            return true;
        }

        if (ex_hash) {
            const char* err = "QOREADDRINFO-GETINFO-ERROR";
            const char* desc = "async address resolution failed";
            QoreValue err_v = ex_hash->getKeyValue("err");
            if (err_v.getType() == NT_STRING) {
                err = err_v.get<const QoreStringNode>()->c_str();
            }
            QoreValue desc_v = ex_hash->getKeyValue("desc");
            if (desc_v.getType() == NT_STRING) {
                desc = desc_v.get<const QoreStringNode>()->c_str();
            }
            action->executeError(err, desc, xsink);
            cleanupAction(xsink);
            return true;
        }

        ValueHolder rv(getOutput(), xsink);
        if (*xsink) {
            action->executeError("QOREADDRINFO-GETINFO-ERROR",
                "async address resolution completed with an exception while retrieving output", xsink);
            cleanupAction(xsink);
            return true;
        }
        if (rv->isNothing()) {
            action->executeError("QOREADDRINFO-GETINFO-ERROR",
                "async address resolution completed without returning address info", xsink);
            cleanupAction(xsink);
            return true;
        }
        if (rv->getType() != NT_LIST) {
            QoreString desc;
            desc.sprintf("expected a list from async address resolution, got '%s'", rv->getFullTypeName());
            action->executeError("QOREADDRINFO-GETINFO-ERROR", desc.c_str(), xsink);
            cleanupAction(xsink);
            return true;
        }
        action->execute(rv.release(), xsink);
        cleanupAction(xsink);
        return true;
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        if (done) {
            return "done";
        }
        return resolver ? "resolving-addrinfo" : "starting-addrinfo";
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

    DLLLOCAL void cleanupAction(ExceptionSink* xsink) {
        if (action) {
            action->deref(xsink);
            action = nullptr;
        }
    }

    std::string node;
    std::string service;
    bool has_node = false;
    bool has_service = false;
    int family = AF_UNSPEC;
    int socktype = SOCK_STREAM;
    int protocol = 0;
    int flags = 0;
    std::unique_ptr<QoreCaresAddrInfoResolver> resolver;
    std::vector<SocketResolvedAddrInfo> addrs;
    AbstractAsyncAction* action = nullptr;
    bool done = false;
};

QoreListNode* qore_socket_resolve_addrinfo_asyncio(ExceptionSink* xsink, const char* node, const char* service,
        int family, int flags, int socktype, int protocol, int timeout_ms) {
    ReferenceHolder<SocketPollOperationBase> poller_holder(
        new QoreSocketControllerResolveAddrInfoPollOperation(node, service, family, socktype, protocol, flags),
        xsink);
    if (*xsink) {
        return nullptr;
    }

    ReferenceHolder<QoreEventNotifier> notifier_holder(new QoreEventNotifier(xsink), xsink);
    if (*xsink || !notifier_holder->isValid()) {
        return nullptr;
    }
    QoreEventNotifier* notifier_raw = *notifier_holder;
    notifier_raw->ref();
    ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_EVENTNOTIFIER, getProgram(), notifier_raw), xsink);
    ReferenceHolder<QoreObject> op_obj(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), *poller_holder), xsink);
    poller_holder->setSelf(*op_obj);
    SocketPollOperationBase* poller = *poller_holder;
    poller_holder.release();

    if (!*xsink) {
        op_obj->setValue("sock", (*sock_obj)->objectRefSelf(), xsink);
        op_obj->setValue("goal", new QoreStringNode("resolve-addrinfo"), xsink);
    }
    if (*xsink) {
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> result(
        qore_socket_exec_poll_operation(*sock_obj, *notifier_holder, *op_obj, timeout_ms, "resolveAddrInfo", xsink),
        xsink);
    if (*xsink) {
        return nullptr;
    }

    ValueHolder rv(poller->getOutput(), xsink);
    if (*xsink) {
        return nullptr;
    }
    if (rv->isNothing()) {
        xsink->raiseException("QOREADDRINFO-GETINFO-ERROR",
            "async address resolution completed without returning address info");
        return nullptr;
    }
    if (rv->getType() != NT_LIST) {
        xsink->raiseException("QOREADDRINFO-GETINFO-ERROR",
            "expected a list from async address resolution, got '%s'", rv->getFullTypeName());
        return nullptr;
    }
    return rv.release().get<QoreListNode>();
}

QoreFuture* qore_socket_resolve_addrinfo_asyncio_future(ExceptionSink* xsink, const char* node, const char* service,
        int family, int flags, int socktype, int protocol, int timeout_ms) {
    ReferenceHolder<QorePromise> promise(new QorePromise(), xsink);
    ReferenceHolder<QoreFuture> future(promise->getFuture(xsink), xsink);
    if (*xsink || !future) {
        return nullptr;
    }

    ReferenceHolder<AbstractAsyncAction> action(new PromiseAction(*promise, nullptr), xsink);
    ReferenceHolder<SocketPollOperationBase> poller_holder(
        new QoreSocketControllerResolveAddrInfoPollOperation(node, service, family, socktype, protocol, flags,
            action.release()),
        xsink);
    if (*xsink) {
        return nullptr;
    }

    ReferenceHolder<QoreEventNotifier> notifier_holder(new QoreEventNotifier(xsink), xsink);
    if (*xsink || !notifier_holder->isValid()) {
        return nullptr;
    }
    QoreEventNotifier* notifier_raw = *notifier_holder;
    notifier_raw->ref();
    ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_EVENTNOTIFIER, getProgram(), notifier_raw), xsink);
    ReferenceHolder<QoreObject> op_obj(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), *poller_holder), xsink);
    poller_holder->setSelf(*op_obj);
    poller_holder.release();

    if (!*xsink) {
        op_obj->setValue("sock", (*sock_obj)->objectRefSelf(), xsink);
        op_obj->setValue("goal", new QoreStringNode("resolve-addrinfo"), xsink);
    }
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

    const std::string& socket_hash = notifier_holder->getIoIdentityHash();

    std::string owner("QoreDNS::getaddrinfo_async:");
    owner += socket_hash;

    std::string key(owner);
    key += ':';
    key += std::to_string(++qore_socket_sync_exec_seq);

    ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollOperationInfo, xsink), xsink);
    info->setKeyValue("sock", (*sock_obj)->objectRefSelf(), xsink);
    info->setKeyValue("spop", (*op_obj)->objectRefSelf(), xsink);
    info->setKeyValue("owner", new QoreStringNode(owner), xsink);
    info->setKeyValue("key", new QoreStringNode(key), xsink);
    info->setKeyValue("thread_key", new QoreStringNode(socket_hash), xsink);
    info->setKeyValue("to", timeout_ms, xsink);
    if (*xsink) {
        return nullptr;
    }

    ReferenceHolder<QoreObject> queue_obj(ctrl->submit(*ctl_obj, info.release(), false, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }

    return future.release();
}

struct SocketResolvedHostByAddrInfo {
    int family = AF_UNSPEC;
    int length = 0;
    int status = ARES_SUCCESS;
    std::string name;
    std::vector<std::string> aliases;
    std::vector<std::string> addresses;
};

static QoreHashNode* qore_socket_resolved_hostbyaddr_to_hash(const SocketResolvedHostByAddrInfo& info) {
    if (info.name.empty()) {
        return nullptr;
    }

    QoreHashNode* h = new QoreHashNode(autoTypeInfo);
    qore_hash_private* hh = qore_hash_private::get(*h);

    hh->setKeyValueIntern("name", new QoreStringNode(info.name));

    QoreListNode* aliases = new QoreListNode(stringTypeInfo);
    for (const std::string& alias : info.aliases) {
        aliases->push(new QoreStringNode(alias), nullptr);
    }
    hh->setKeyValueIntern("aliases", aliases);

    switch (info.family) {
        case AF_INET:
            hh->setKeyValueIntern("typename", new QoreStringNode("ipv4"));
            hh->setKeyValueIntern("type", AF_INET);
            hh->setKeyValueIntern("len", info.length ? info.length : 4);
            break;
        case AF_INET6:
            hh->setKeyValueIntern("typename", new QoreStringNode("ipv6"));
            hh->setKeyValueIntern("type", AF_INET6);
            hh->setKeyValueIntern("len", info.length ? info.length : 16);
            break;
        default:
            hh->setKeyValueIntern("typename", new QoreStringNode("unknown"));
            break;
    }

    QoreListNode* addresses = new QoreListNode(stringTypeInfo);
    for (const std::string& address : info.addresses) {
        addresses->push(new QoreStringNode(address), nullptr);
    }
    hh->setKeyValueIntern("addresses", addresses);

    return h;
}

class QoreCaresHostByAddrResolver {
public:
    DLLLOCAL QoreCaresHostByAddrResolver(const struct sockaddr_storage& addr, socklen_t len)
            : addr(addr), len(len) {
        info.family = addr.ss_family;
    }

    DLLLOCAL ~QoreCaresHostByAddrResolver() {
        if (channel) {
            ares_destroy(channel);
            channel = nullptr;
        }
    }

    DLLLOCAL int continuePoll(ExceptionSink* xsink) {
        if (!started && start(xsink)) {
            return -1;
        }
        if (done) {
            return 0;
        }

        process();
        return done ? 0 : 1;
    }

    DLLLOCAL void getExtraFds(std::vector<std::pair<int, int>>& fds) const {
        if (!channel || done) {
            return;
        }

        for (auto& [fd, events] : fd_events) {
            fds.push_back({fd, events});
        }
    }

    DLLLOCAL int getPollTimeoutMs() const {
        if (!channel || done) {
            return -1;
        }
        struct timeval tv;
        struct timeval* tvp = ares_timeout(channel, nullptr, &tv);
        if (!tvp) {
            return -1;
        }
        int64_t ms = static_cast<int64_t>(tvp->tv_sec) * 1000 + (tvp->tv_usec + 999) / 1000;
        return ms <= 0 ? 1 : static_cast<int>(std::min<int64_t>(ms, INT_MAX));
    }

    DLLLOCAL const SocketResolvedHostByAddrInfo& getInfo() const {
        return info;
    }

private:
    DLLLOCAL int start(ExceptionSink* xsink) {
        if (qore_cares_library_init(xsink)) {
            return -1;
        }

        if (addr.ss_family != AF_INET && addr.ss_family != AF_INET6) {
            xsink->raiseException("GETHOSTBYADDR-ERROR", "invalid address family for reverse lookup: %d",
                addr.ss_family);
            return -1;
        }

        struct ares_options options = {};
        options.sock_state_cb = sockStateCallback;
        options.sock_state_cb_data = this;

        int rc = ares_init_options(&channel, &options, ARES_OPT_SOCK_STATE_CB);
        if (rc != ARES_SUCCESS) {
            xsink->raiseException("GETHOSTBYADDR-ERROR", "ares_init_options() failed: %s", ares_strerror(rc));
            return -1;
        }

        void* raw_addr = qore_get_in_addr(reinterpret_cast<struct sockaddr*>(&addr));
        info.length = addr.ss_family == AF_INET ? sizeof(struct in_addr) : sizeof(struct in6_addr);

        char ifname[INET6_ADDRSTRLEN];
        if (inet_ntop(addr.ss_family, raw_addr, ifname, sizeof(ifname))) {
            info.addresses.push_back(ifname);
        }

        started = true;
        ares_gethostbyaddr(channel, raw_addr, info.length, addr.ss_family, callback, this);
        return 0;
    }

    DLLLOCAL void process() {
        std::vector<std::pair<int, int>> fds;
        getExtraFds(fds);
        if (fds.empty()) {
            ares_process_fd(channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
            return;
        }

        for (auto& [fd, events] : fds) {
            ares_socket_t rfd = (events & SOCK_POLLIN) ? fd : ARES_SOCKET_BAD;
            ares_socket_t wfd = (events & SOCK_POLLOUT) ? fd : ARES_SOCKET_BAD;
            ares_process_fd(channel, rfd, wfd);
            if (done) {
                break;
            }
        }
    }

    DLLLOCAL void complete(int new_status, struct hostent* he) {
        info.status = new_status;
        if (info.status == ARES_SUCCESS && he) {
            info.family = he->h_addrtype;
            info.length = he->h_length;
            if (he->h_name && *he->h_name) {
                info.name = he->h_name;
            }
            if (he->h_aliases) {
                for (char** a = he->h_aliases; *a; ++a) {
                    info.aliases.push_back(*a);
                }
            }
            if (he->h_addr_list) {
                std::vector<std::string> addresses;
                char buf[INET6_ADDRSTRLEN];
                for (char** a = he->h_addr_list; *a; ++a) {
                    if (inet_ntop(he->h_addrtype, *a, buf, sizeof(buf))) {
                        addresses.push_back(buf);
                    }
                }
                if (!addresses.empty()) {
                    info.addresses = std::move(addresses);
                }
            }
        }
        done = true;
    }

    DLLLOCAL void updateFd(ares_socket_t socket_fd, int readable, int writable) {
        int events = 0;
        if (readable) {
            events |= SOCK_POLLIN;
        }
        if (writable) {
            events |= SOCK_POLLOUT;
        }
        int fd = static_cast<int>(socket_fd);
        if (events) {
            fd_events[fd] = events;
        } else {
            fd_events.erase(fd);
        }
    }

    DLLLOCAL static void callback(void* arg, int status, int, struct hostent* he) {
        reinterpret_cast<QoreCaresHostByAddrResolver*>(arg)->complete(status, he);
    }

    DLLLOCAL static void sockStateCallback(void* arg, ares_socket_t socket_fd, int readable, int writable) {
        reinterpret_cast<QoreCaresHostByAddrResolver*>(arg)->updateFd(socket_fd, readable, writable);
    }

    struct sockaddr_storage addr = {};
    socklen_t len = 0;
    ares_channel_t* channel = nullptr;
    std::unordered_map<int, int> fd_events;
    SocketResolvedHostByAddrInfo info;
    bool started = false;
    bool done = false;
};

class QoreSocketControllerResolveHostByAddrPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketControllerResolveHostByAddrPollOperation(const struct sockaddr_storage& addr, socklen_t len)
            : addr(addr), len(len) {
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
        if (!resolver) {
            resolver = std::make_unique<QoreCaresHostByAddrResolver>(addr, len);
        }

        int rc = resolver->continuePoll(xsink);
        if (*xsink || rc < 0) {
            done = true;
            resolver.reset();
            return nullptr;
        }
        if (rc) {
            return getResolverPollInfo(xsink);
        }

        info = resolver->getInfo();
        resolver.reset();
        done = true;
        return nullptr;
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return done && info.status == ARES_SUCCESS ? qore_socket_resolved_hostbyaddr_to_hash(info) : QoreValue();
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        if (done) {
            return "done";
        }
        return resolver ? "resolving-hostbyaddr" : "starting-hostbyaddr";
    }

    DLLLOCAL int getStatus() const {
        return info.status;
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

    struct sockaddr_storage addr = {};
    socklen_t len = 0;
    std::unique_ptr<QoreCaresHostByAddrResolver> resolver;
    SocketResolvedHostByAddrInfo info;
    bool done = false;
};

QoreHashNode* qore_socket_resolve_hostbyaddr_asyncio(ExceptionSink* xsink, const struct sockaddr_storage& addr,
        socklen_t len, int timeout_ms) {
    ReferenceHolder<SocketPollOperationBase> poller_holder(
        new QoreSocketControllerResolveHostByAddrPollOperation(addr, len), xsink);
    if (*xsink) {
        return nullptr;
    }

    ReferenceHolder<QoreEventNotifier> notifier_holder(new QoreEventNotifier(xsink), xsink);
    if (*xsink || !notifier_holder->isValid()) {
        return nullptr;
    }
    QoreEventNotifier* notifier_raw = *notifier_holder;
    notifier_raw->ref();
    ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_EVENTNOTIFIER, getProgram(), notifier_raw), xsink);
    ReferenceHolder<QoreObject> op_obj(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), *poller_holder), xsink);
    poller_holder->setSelf(*op_obj);
    QoreSocketControllerResolveHostByAddrPollOperation* poller =
        static_cast<QoreSocketControllerResolveHostByAddrPollOperation*>(*poller_holder);
    poller_holder.release();

    if (!*xsink) {
        op_obj->setValue("sock", (*sock_obj)->objectRefSelf(), xsink);
        op_obj->setValue("goal", new QoreStringNode("resolve-hostbyaddr"), xsink);
    }
    if (*xsink) {
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> result(
        qore_socket_exec_poll_operation(*sock_obj, *notifier_holder, *op_obj, timeout_ms, "resolveHostByAddr",
            xsink),
        xsink);
    if (*xsink) {
        return nullptr;
    }

    ValueHolder rv(poller->getOutput(), xsink);
    if (*xsink) {
        return nullptr;
    }
    if (rv->isNothing()) {
        xsink->raiseException("GETHOSTBYADDR-ERROR", "gethostbyaddr() failed: %s",
            ares_strerror(poller->getStatus()));
        return nullptr;
    }
    if (rv->getType() != NT_HASH) {
        xsink->raiseException("GETHOSTBYADDR-ERROR", "expected a hash from async reverse address resolution, got '%s'",
            rv->getFullTypeName());
        return nullptr;
    }
    return rv.release().get<QoreHashNode>();
}

class QoreSocketControllerResolveNameInfoPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL QoreSocketControllerResolveNameInfoPollOperation(const struct sockaddr_storage& addr, socklen_t len,
            AbstractAsyncAction* action)
            : addr(addr), len(len), action(action) {
    }

    DLLLOCAL virtual ~QoreSocketControllerResolveNameInfoPollOperation() {
        ExceptionSink xsink;
        cleanupAction(&xsink);
        if (xsink) {
            xsink.clear();
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return done;
    }

    DLLLOCAL virtual void abort(ExceptionSink*) override {
        done = true;
        resolver.reset();
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        if (done) {
            return nullptr;
        }
        if (!resolver) {
            resolver = std::make_unique<QoreCaresNameInfoResolver>(addr, len);
        }

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
        return done
            ? qore_socket_private::makeAddrInfo(addr, len, std::string(), hostname.empty() ? nullptr : hostname.c_str())
            : QoreValue();
    }

    DLLLOCAL virtual bool handleCompletion(bool canceled, const QoreHashNode* ex_hash,
            ExceptionSink* xsink) override {
        if (!action) {
            return false;
        }

        if (canceled) {
            action->executeError("QOREADDRINFO-CANCELLED", "async reverse address resolution was canceled", xsink);
            cleanupAction(xsink);
            return true;
        }

        if (ex_hash) {
            const char* err = "QOREADDRINFO-GETNAMEINFO-ERROR";
            const char* desc = "async reverse address resolution failed";
            QoreValue err_v = ex_hash->getKeyValue("err");
            if (err_v.getType() == NT_STRING) {
                err = err_v.get<const QoreStringNode>()->c_str();
            }
            QoreValue desc_v = ex_hash->getKeyValue("desc");
            if (desc_v.getType() == NT_STRING) {
                desc = desc_v.get<const QoreStringNode>()->c_str();
            }
            action->executeError(err, desc, xsink);
            cleanupAction(xsink);
            return true;
        }

        ValueHolder rv(getOutput(), xsink);
        if (*xsink) {
            action->executeError("QOREADDRINFO-GETNAMEINFO-ERROR",
                "async reverse address resolution completed with an exception while retrieving output", xsink);
            cleanupAction(xsink);
            return true;
        }
        if (rv->isNothing()) {
            action->executeError("QOREADDRINFO-GETNAMEINFO-ERROR",
                "async reverse address resolution completed without returning address info", xsink);
            cleanupAction(xsink);
            return true;
        }
        if (rv->getType() != NT_HASH) {
            QoreString desc;
            desc.sprintf("expected a hash from async reverse address resolution, got '%s'", rv->getFullTypeName());
            action->executeError("QOREADDRINFO-GETNAMEINFO-ERROR", desc.c_str(), xsink);
            cleanupAction(xsink);
            return true;
        }
        action->execute(rv.release(), xsink);
        cleanupAction(xsink);
        return true;
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        if (done) {
            return "done";
        }
        return resolver ? "resolving-nameinfo" : "starting-nameinfo";
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

    DLLLOCAL void cleanupAction(ExceptionSink* xsink) {
        if (action) {
            action->deref(xsink);
            action = nullptr;
        }
    }

    struct sockaddr_storage addr = {};
    socklen_t len = 0;
    std::unique_ptr<QoreCaresNameInfoResolver> resolver;
    std::string hostname;
    AbstractAsyncAction* action = nullptr;
    bool done = false;
};

QoreFuture* qore_socket_resolve_nameinfo_asyncio_future(ExceptionSink* xsink, const struct sockaddr_storage& addr,
        socklen_t len, int timeout_ms) {
    ReferenceHolder<QorePromise> promise(new QorePromise(), xsink);
    ReferenceHolder<QoreFuture> future(promise->getFuture(xsink), xsink);
    if (*xsink || !future) {
        return nullptr;
    }

    ReferenceHolder<AbstractAsyncAction> action(new PromiseAction(*promise, nullptr), xsink);
    ReferenceHolder<SocketPollOperationBase> poller_holder(
        new QoreSocketControllerResolveNameInfoPollOperation(addr, len, action.release()), xsink);
    if (*xsink) {
        return nullptr;
    }

    ReferenceHolder<QoreEventNotifier> notifier_holder(new QoreEventNotifier(xsink), xsink);
    if (*xsink || !notifier_holder->isValid()) {
        return nullptr;
    }
    QoreEventNotifier* notifier_raw = *notifier_holder;
    notifier_raw->ref();
    ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_EVENTNOTIFIER, getProgram(), notifier_raw), xsink);
    ReferenceHolder<QoreObject> op_obj(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), *poller_holder), xsink);
    poller_holder->setSelf(*op_obj);
    poller_holder.release();

    if (!*xsink) {
        op_obj->setValue("sock", (*sock_obj)->objectRefSelf(), xsink);
        op_obj->setValue("goal", new QoreStringNode("resolve-nameinfo"), xsink);
    }
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

    const std::string& socket_hash = notifier_holder->getIoIdentityHash();

    std::string owner("QoreDNS::getnameinfo_async:");
    owner += socket_hash;

    std::string key(owner);
    key += ':';
    key += std::to_string(++qore_socket_sync_exec_seq);

    ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollOperationInfo, xsink), xsink);
    info->setKeyValue("sock", (*sock_obj)->objectRefSelf(), xsink);
    info->setKeyValue("spop", (*op_obj)->objectRefSelf(), xsink);
    info->setKeyValue("owner", new QoreStringNode(owner), xsink);
    info->setKeyValue("key", new QoreStringNode(key), xsink);
    info->setKeyValue("thread_key", new QoreStringNode(socket_hash), xsink);
    info->setKeyValue("to", timeout_ms, xsink);
    if (*xsink) {
        return nullptr;
    }

    ReferenceHolder<QoreObject> queue_obj(ctrl->submit(*ctl_obj, info.release(), false, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }

    return future.release();
}

QoreCaresNameInfoResolver::QoreCaresNameInfoResolver(const struct sockaddr_storage& addr, socklen_t len, int flags)
        : addr(addr), len(len), flags(flags) {
}

QoreCaresNameInfoResolver::~QoreCaresNameInfoResolver() {
    if (channel) {
        ares_destroy(channel);
        channel = nullptr;
    }
}

int QoreCaresNameInfoResolver::continuePoll(ExceptionSink* xsink) {
    if (!started && start(xsink)) {
        return -1;
    }
    if (done) {
        return 0;
    }

    process();
    return done ? 0 : 1;
}

void QoreCaresNameInfoResolver::getExtraFds(std::vector<std::pair<int, int>>& fds) const {
    if (!channel || done) {
        return;
    }

    for (auto& [fd, events] : fd_events) {
        fds.push_back({fd, events});
    }
}

int QoreCaresNameInfoResolver::getPollTimeoutMs() const {
    if (!channel || done) {
        return -1;
    }

    struct timeval tv;
    struct timeval* tvp = ares_timeout(channel, nullptr, &tv);
    if (!tvp) {
        return -1;
    }
    int64_t ms = static_cast<int64_t>(tvp->tv_sec) * 1000 + (tvp->tv_usec + 999) / 1000;
    return ms <= 0 ? 1 : static_cast<int>(std::min<int64_t>(ms, INT_MAX));
}

int QoreCaresNameInfoResolver::start(ExceptionSink* xsink) {
    if (qore_cares_library_init(xsink)) {
        return -1;
    }

    struct ares_options options = {};
    options.sock_state_cb = sockStateCallback;
    options.sock_state_cb_data = this;

    int rc = ares_init_options(&channel, &options, ARES_OPT_SOCK_STATE_CB);
    if (rc != ARES_SUCCESS) {
        xsink->raiseException("QOREADDRINFO-GETNAMEINFO-ERROR", "ares_init_options() failed: %s",
            ares_strerror(rc));
        return -1;
    }

    started = true;
    ares_socklen_t alen = len ? static_cast<ares_socklen_t>(len)
        : static_cast<ares_socklen_t>(qore_get_in_len(reinterpret_cast<struct sockaddr*>(&addr)));
    ares_getnameinfo(channel, reinterpret_cast<const struct sockaddr*>(&addr), alen, flags, callback, this);
    return 0;
}

void QoreCaresNameInfoResolver::process() {
    std::vector<std::pair<int, int>> fds;
    getExtraFds(fds);
    if (fds.empty()) {
        ares_process_fd(channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
        return;
    }

    for (auto& [fd, events] : fds) {
        ares_socket_t rfd = (events & SOCK_POLLIN) ? fd : ARES_SOCKET_BAD;
        ares_socket_t wfd = (events & SOCK_POLLOUT) ? fd : ARES_SOCKET_BAD;
        ares_process_fd(channel, rfd, wfd);
        if (done) {
            break;
        }
    }
}

void QoreCaresNameInfoResolver::callback(void* arg, int status, int, char* node, char*) {
    reinterpret_cast<QoreCaresNameInfoResolver*>(arg)->complete(status, node);
}

void QoreCaresNameInfoResolver::sockStateCallback(void* arg, ares_socket_t socket_fd, int readable, int writable) {
    reinterpret_cast<QoreCaresNameInfoResolver*>(arg)->updateFd(socket_fd, readable, writable);
}

void QoreCaresNameInfoResolver::updateFd(ares_socket_t socket_fd, int readable, int writable) {
    int events = 0;
    if (readable) {
        events |= SOCK_POLLIN;
    }
    if (writable) {
        events |= SOCK_POLLOUT;
    }
    int fd = static_cast<int>(socket_fd);
    if (events) {
        fd_events[fd] = events;
    } else {
        fd_events.erase(fd);
    }
}

void QoreCaresNameInfoResolver::complete(int new_status, char* node) {
    status = new_status;
    if (status == ARES_SUCCESS && node && *node) {
        hostname = node;
    }
    done = true;
}

// --- Happy Eyeballs (RFC 8305) async poll state ---

SocketConnectInetHappyEyeballsPollState::SocketConnectInetHappyEyeballsPollState(ExceptionSink* xsink,
        qore_socket_private* sock, const char* host, const char* service, int family, int type, int protocol,
        QoreSandboxManager* sandbox_manager)
        : sock(sock), sandbox_manager(qore_socket_ref_sandbox_manager(sandbox_manager)), host(host),
            service(service) {
    assert(xsink);

    this->family = q_get_af(family);
    this->type = q_get_sock_type(type);
    this->protocol = protocol;

    // close socket if already open
    qore_socket_close_private_from_controller(sock);

    sock->do_resolve_event(host, service);
}

SocketConnectInetHappyEyeballsPollState::~SocketConnectInetHappyEyeballsPollState() {
    closeAllFds();
}

int SocketConnectInetHappyEyeballsPollState::continuePoll(ExceptionSink* xsink) {
    if (he_state == HEBS_RESOLVING) {
        int rc = continueResolve(xsink);
        if (*xsink || rc) {
            return *xsink ? -1 : SOCK_POLLIN;
        }

        rc = startNextConnect(xsink);
        if (rc < 0 && !*xsink) {
            qore_socket_error(xsink, "SOCKET-CONNECT-ERROR", "error in connect()", nullptr, host.c_str(),
                service.c_str());
        }
        if (rc <= 0) {
            if (rc == 0) {
                he_state = HEBS_CONNECTED;
                assignWinner(xsink);
            }
            return rc;
        }
        he_state = HEBS_RACING;
        return SOCK_POLLOUT;
    }

    if (he_state == HEBS_CONNECTED) {
        return 0;
    }

    // Check all active attempts for completion
    for (size_t i = 0; i < active_attempts.size(); ++i) {
        if (active_attempts[i].fd == QORE_INVALID_SOCKET) {
            continue;
        }
        int rc = checkAttempt(i);
        if (rc == 0) {
            // Connected!
            winning_idx = (int)i;
            he_state = HEBS_CONNECTED;
            assignWinner(xsink);
            return *xsink ? -1 : 0;
        }
        if (rc < 0) {
            // Failed — close this fd
            bool was_primary = (active_attempts[i].fd == sock->sock);
#ifdef _Q_WINDOWS
            closesocket(active_attempts[i].fd);
#else
            ::close(active_attempts[i].fd);
#endif
            active_attempts[i].fd = QORE_INVALID_SOCKET;
            if (was_primary) {
                // Assign another active fd as primary so isOpen() remains true
                sock->sock = QORE_INVALID_SOCKET;
                for (auto& a : active_attempts) {
                    if (a.fd != QORE_INVALID_SOCKET) {
                        sock->sock = a.fd;
                        sock->resetCloseInterrupt();
                        break;
                    }
                }
            }
        }
    }

    // Check if any attempts are still in progress
    bool any_active = false;
    for (auto& a : active_attempts) {
        if (a.fd != QORE_INVALID_SOCKET) {
            any_active = true;
            break;
        }
    }

    // Start next connection attempt when:
    // 1. The 250ms timer has fired (he_state == HEBS_RACING), OR
    // 2. All current attempts have already failed (need to try next immediately)
    // On the first call (HEBS_FIRST_CONNECT) with an attempt still in progress,
    // we return SOCK_POLLOUT to honor the 250ms stagger before starting the next address.
    if (next_addr_idx < sorted_addrs.size() && (he_state == HEBS_RACING || !any_active)) {
        int rc = startNextConnect(xsink);
        if (*xsink) {
            closeAllFds();
            sock->sock = QORE_INVALID_SOCKET;
            return -1;
        }
        if (rc == 0) {
            // Immediate connection
            he_state = HEBS_CONNECTED;
            assignWinner(xsink);
            return *xsink ? -1 : 0;
        }
    }

    // Recheck if any attempts are still active (startNextConnect may have added new ones)
    any_active = false;
    for (auto& a : active_attempts) {
        if (a.fd != QORE_INVALID_SOCKET) {
            any_active = true;
            break;
        }
    }

    if (!any_active) {
        sock->sock = QORE_INVALID_SOCKET;
        qore_socket_error(xsink, "SOCKET-CONNECT-ERROR", "error in connect()", nullptr, host.c_str(), service.c_str());
        return -1;
    }

    he_state = HEBS_RACING;
    return SOCK_POLLOUT;
}

void SocketConnectInetHappyEyeballsPollState::getExtraFds(std::vector<std::pair<int, int>>& fds) const {
    if (he_state == HEBS_RESOLVING) {
        if (resolver) {
            resolver->getExtraFds(fds);
        }
        return;
    }

    // Return all active fds that are NOT the primary sock->sock fd
    for (auto& a : active_attempts) {
        if (a.fd != QORE_INVALID_SOCKET && a.fd != sock->sock) {
            fds.push_back({a.fd, SOCK_POLLOUT});
        }
    }
}

int SocketConnectInetHappyEyeballsPollState::getPollTimeoutMs() const {
    if (he_state == HEBS_RESOLVING) {
        return resolver ? resolver->getPollTimeoutMs() : 1;
    }
    return he_state == HEBS_RACING ? HAPPY_EYEBALLS_DELAY_MS : -1;
}

int SocketConnectInetHappyEyeballsPollState::getPrimaryPollEvents(int rc) const {
    return he_state == HEBS_RESOLVING ? 0 : rc;
}

int SocketConnectInetHappyEyeballsPollState::continueResolve(ExceptionSink* xsink) {
    if (!resolver) {
        resolver.reset(new QoreCaresAddrInfoResolver(host, service, family, type, protocol));
    }

    int rc = resolver->continuePoll(xsink);
    if (*xsink || rc) {
        return *xsink ? -1 : rc;
    }

    return finishResolve(xsink);
}

int SocketConnectInetHappyEyeballsPollState::finishResolve(ExceptionSink* xsink) {
    addrs = resolver->getAddresses();
    resolver.reset();
    if (addrs.empty()) {
        xsink->raiseException("QOREADDRINFO-GETINFO-ERROR",
            "ares_getaddrinfo(node: '%s', service: '%s', address_family: %d='%s') returned no addresses",
            host.c_str(), service.c_str(), family, q_af_to_str(family));
        return -1;
    }

    // emit all "resolved" events
    if (sock->hasEventQueue()) {
        for (auto& ai : addrs) {
            sock->do_resolved_event(reinterpret_cast<const struct sockaddr*>(&ai.addr));
        }
    }

    prt = q_get_port_from_addr(reinterpret_cast<const struct sockaddr*>(&addrs.front().addr));

    std::vector<size_t> v6, v4, other;
    for (size_t i = 0; i < addrs.size(); ++i) {
        if (addrs[i].family == AF_INET6) {
            v6.push_back(i);
        } else if (addrs[i].family == AF_INET) {
            v4.push_back(i);
        } else {
            other.push_back(i);
        }
    }

    multi_family = !v6.empty() && !v4.empty();

    size_t i6 = 0, i4 = 0;
    while (i6 < v6.size() || i4 < v4.size()) {
        if (i6 < v6.size()) {
            sorted_addrs.push_back(v6[i6++]);
        }
        if (i4 < v4.size()) {
            sorted_addrs.push_back(v4[i4++]);
        }
    }
    for (size_t i : other) {
        sorted_addrs.push_back(i);
    }

    he_state = HEBS_FIRST_CONNECT;

    printd(5, "SocketConnectInetHappyEyeballsPollState::finishResolve() host: %s service: %s addrs: %zu "
        "multi_family: %d\n", host.c_str(), service.c_str(), sorted_addrs.size(), (int)multi_family);
    return 0;
}

int SocketConnectInetHappyEyeballsPollState::startNextConnect(ExceptionSink* xsink) {
    while (next_addr_idx < sorted_addrs.size()) {
        SocketResolvedAddrInfo& p = addrs[sorted_addrs[next_addr_idx]];

        // Check sandbox network security restrictions
        if (sandbox_manager) {
            int proto = (p.socktype == SOCK_STREAM) ? QSEC_NET_TCP :
                        (p.socktype == SOCK_DGRAM) ? QSEC_NET_UDP : QSEC_NET_ALL;
            if (!sandbox_manager->checkNetworkAccess(reinterpret_cast<const struct sockaddr*>(&p.addr), p.addrlen,
                    proto, xsink)) {
                return -1;
            }
        }

        sock->do_connect_event(p.family, reinterpret_cast<const struct sockaddr*>(&p.addr), host.c_str(),
            service.c_str(), prt);

        int fd = create_nonblocking_socket(p.family, p.socktype, p.protocol);
        if (fd == QORE_INVALID_SOCKET) {
            ++next_addr_idx;
            continue;
        }

        int rc;
        while (true) {
            rc = ::connect(fd, reinterpret_cast<const struct sockaddr*>(&p.addr), p.addrlen);
            if (!rc || sock_get_error() != EINTR) {
                break;
            }
        }
        if (rc == 0) {
            // Immediate connection
            ConnAttempt a;
            a.fd = fd;
            a.addr_idx = next_addr_idx;
            active_attempts.push_back(a);
            winning_idx = (int)(active_attempts.size() - 1);
            ++next_addr_idx;
            return 0;
        }

#ifdef _Q_WINDOWS
        if (sock_get_error() != EAGAIN) {
            closesocket(fd);
            ++next_addr_idx;
            continue;
        }
#else
        if (errno != EINPROGRESS && errno != EAGAIN) {
            ::close(fd);
            ++next_addr_idx;
            continue;
        }
#endif

        ConnAttempt a;
        a.fd = fd;
        a.addr_idx = next_addr_idx;
        active_attempts.push_back(a);
        ++next_addr_idx;

        // Assign first racing fd to sock->sock so isOpen() returns true
        if (sock->sock == QORE_INVALID_SOCKET) {
            sock->sock = fd;
            sock->resetCloseInterrupt();
        }

        return 1; // in progress
    }

    return -1; // no more addresses
}

int SocketConnectInetHappyEyeballsPollState::checkAttempt(size_t idx) {
    assert(idx < active_attempts.size());
    int fd = active_attempts[idx].fd;
    assert(fd != QORE_INVALID_SOCKET);

    return qore_socket_check_connect_ready(fd);
}

void SocketConnectInetHappyEyeballsPollState::assignWinner(ExceptionSink* xsink) {
    assert(winning_idx >= 0 && winning_idx < (int)active_attempts.size());
    auto& winner = active_attempts[winning_idx];
    assert(winner.fd != QORE_INVALID_SOCKET);

    SocketResolvedAddrInfo& wp = addrs[sorted_addrs[winner.addr_idx]];

    // Assign winning fd to socket (may already be sock->sock if first attempt won)
    sock->sock = winner.fd;
    winner.fd = QORE_INVALID_SOCKET;
    sock->resetCloseInterrupt();
    sock->sfamily = wp.family;
    sock->stype = wp.socktype;
    sock->sprot = wp.protocol;
    sock->port = prt;

    // Close all other fds
    closeAllFds();

    sock->confirmConnected(host.c_str());

    printd(5, "SocketConnectInetHappyEyeballsPollState::assignWinner() host: %s family: %s\n",
        host.c_str(), q_af_to_str(wp.family));
}

void SocketConnectInetHappyEyeballsPollState::closeAllFds() {
    for (auto& a : active_attempts) {
        if (a.fd != QORE_INVALID_SOCKET) {
            // Don't close the fd if it's currently assigned to sock->sock
            // (that will be handled by sock->close() or by assignWinner)
            if (a.fd != sock->sock) {
#ifdef _Q_WINDOWS
                closesocket(a.fd);
#else
                ::close(a.fd);
#endif
            }
            a.fd = QORE_INVALID_SOCKET;
        }
    }
}

#ifndef _Q_WINDOWS
SocketConnectUnixPollState::SocketConnectUnixPollState(ExceptionSink* xsink, qore_socket_private* sock,
        const char* name, int sock_type, int protocol, QoreSandboxManager* sandbox_manager)
        : sock(sock), name(name) {
    assert(xsink);

    // close socket if already open
    qore_socket_close_private_from_controller(sock);
    assert(sock->sock == QORE_INVALID_SOCKET);

    addr.sun_family = AF_UNIX;
    // copy path and terminate if necessary
    strncpy(addr.sun_path, name, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

    // Check sandbox network security restrictions for UNIX sockets
    SimpleRefHolder<QoreSandboxManager> smh(qore_socket_ref_sandbox_manager(sandbox_manager));
    if (smh) {
        if (!smh->checkNetworkAccess((const struct sockaddr*)&addr, sizeof(struct sockaddr_un),
                QSEC_NET_UNIX, xsink)) {
            return;
        }
    }

    if ((sock->sock = create_nonblocking_socket(AF_UNIX, sock_type, protocol)) == QORE_SOCKET_ERROR) {
        xsink->raiseErrnoException("SOCKET-CONNECT-ERROR", errno, "error connecting to UNIX socket: '%s'", name);
        return;
    }
    sock->resetCloseInterrupt();

    sock->do_connect_event(AF_UNIX, (sockaddr*)&addr, name);
}

/** returns:
    - SOCK_POLLIN = wait for read and call this again
    - SOCK_POLLOUT = wait for write and call this again
    - 0 = done
    - < 1 = error (exception raised)
*/
int SocketConnectUnixPollState::continuePoll(ExceptionSink* xsink) {
    // set non-blocking
    OptionalNonBlockingHelper nbh(*sock, true, xsink);
    if (*xsink) {
        return -1;
    }

    while (true) {
        if (state == SCIPS_CONNECT) {
            int rc = doConnect(xsink);
            //printd(5, "SocketConnectUnixPollState::continuePoll() doConnect() returned %d (ex: %d)\n", rc,
            //    (int)*xsink);
            if (*xsink) {
                sock->close_and_reset();
                return -1;
            }
            if (rc) {
                sock->close_and_reset();
                qore_socket_error(xsink, "SOCKET-CONNECT-ERROR", "error in connect()", 0, name.c_str());
                return -1;
            }

            // connect successful; do an immediate check for a connection
            state = SCIPS_CHECK_CONNECT;
        }

        if (state == SCIPS_CHECK_CONNECT) {
            int rc = checkConnection(xsink);
            //printd(5, "SocketConnectUnixPollState::continuePoll() checkConnection() returned %d (ex: %d)\n", rc,
            //    (int)*xsink);
            if (*xsink) {
                sock->close_and_reset();
                return -1;
            }

            if (rc == 1) {
                return SOCK_POLLOUT;
            }

            if (rc < 0) {
                sock->close_and_reset();
                qore_socket_error(xsink, "SOCKET-CONNECT-ERROR", "error in connect()", 0, name.c_str());
                return -1;
            }
        }

        break;
    }
    return 0;
}

int SocketConnectUnixPollState::doConnect(ExceptionSink* xsink) {
    while (true) {
        if (!::connect(sock->sock, (const sockaddr*)&addr, sizeof(struct sockaddr_un))) {
            return 0;
        }

        // try again if we were interrupted by a signal
        if (errno == EINTR) {
            continue;
        }

        if (errno != EINPROGRESS && errno != EAGAIN) {
            return -1;
        }

        break;
    }
    return 0;
}

// returns 0 = connected, 1 = try again, -1 = error
int SocketConnectUnixPollState::checkConnection(ExceptionSink* xsink) {
    assert(!*xsink);
    assert(sock->sock);

    // The async I/O controller calls this state after writable readiness.
    // When continuePoll() reaches this immediately after EINPROGRESS, the
    // SO_ERROR / zero-byte send checks still report "try again" without
    // running a second raw poll/select wait inside the poll operation.
    int rc = qore_socket_check_connect_ready(sock->sock);
    if (rc) {
        return rc;
    }

    // connected successfully within the timeout period
    sock->socketname = addr.sun_path;
    sock->sfamily = AF_UNIX;
    sock->confirmConnected(nullptr);
    return 0;
}
#endif

SocketConnectSslPollState::SocketConnectSslPollState(ExceptionSink* xsink, qore_socket_private* sock,
        QoreSSLCertificate* cert, QoreSSLPrivateKey* pkey) : sock(sock) {
    assert(sock->sock);
    assert(!sock->ssl);
    SSLSocketHelperHelper sshh(sock, true);

    sock->do_start_ssl_event();
    // issue #3053: send target hostname to support SNI
    const char* sni_target_host = sock->client_target.empty() ? "" : sock->client_target.c_str();
    if (sock->ssl->setClient(xsink, "connectSsl", sni_target_host, sock->sock, cert, pkey)) {
        sshh.error();
        return;
    }

    // Set ALPN protocols if configured (for HTTP/2 support).
    // This must match the async SSL poll setup paths.
    if (!sock->alpn_protocols.empty()) {
        sock->ssl->setAlpnProtocols(sock->alpn_protocols);
    }
}

/** returns:
    - SOCK_POLLIN = wait for read and call this again
    - SOCK_POLLOUT = wait for write and call this again
    - 0 = done
    - < 1 = error (exception raised)
*/
int SocketConnectSslPollState::continuePoll(ExceptionSink* xsink) {
    return sock->ssl->startConnect(xsink);
}

SocketSendPollState::SocketSendPollState(ExceptionSink* xsink, qore_socket_private* sock, const char* data,
        size_t size) : sock(sock), data(data), size(size) {
}

/** returns:
    - SOCK_POLLIN = wait for read and call this again
    - SOCK_POLLOUT = wait for write and call this again
    - 0 = done
    - < 0 = error (exception raised)
*/
int SocketSendPollState::continuePoll(ExceptionSink* xsink) {
    OptionalNonBlockingHelper nbh(*sock, true, xsink);
    if (*xsink) {
        return -1;
    }

    // do not allow more than max_nonblock_ops loops at a time
    unsigned loop = 0;

    while (true) {
        ssize_t rc;
        if (sock->ssl) {
            size_t real_io = 0;
            rc = sock->ssl->doNonBlockingIo(xsink, "send", const_cast<char*>(data + sent), size - sent,
                SslAction::WRITE, real_io);
            if (*xsink) {
                assert(!real_io);
                return -1;
            }
            if (real_io) {
                sent += real_io;
                if (sent == size) {
                    break;
                }
                if (rc) {
                    return rc;
                }
                // do not allow more than max_nonblock_ops loops at a time
                if (++loop >= max_nonblock_ops) {
                    return SOCK_POLLOUT;
                }
                // do another send
                continue;
            }
            return rc;
        } else {
            rc = ::send(sock->sock, data + sent, size - sent, 0);
            //printd(5, "SocketSendPollState::continuePoll() left: %ld rc: %ld errno: %d)\n", size - sent, rc,
            //    errno);
            if (*xsink) {
                return -1;
            }
            if (rc >= 0) {
                sent += rc;
                if (sent == size) {
                    break;
                }
                // do not allow more than max_nonblock_ops loops at a time
                if (++loop >= max_nonblock_ops) {
                    return SOCK_POLLOUT;
                }
                // do another send
                continue;
            }
            sock_get_error();
            if (errno == EINTR) {
                // do not allow more than max_nonblock_ops loops at a time
                if (++loop >= max_nonblock_ops) {
                    return SOCK_POLLOUT;
                }
                continue;
            }
            if (errno == EAGAIN
#ifdef EWOULDBLOCK
                || errno == EWOULDBLOCK
#endif
            ) {
                return SOCK_POLLOUT;
            }
            xsink->raiseErrnoException("SOCKET-SEND-ERROR", errno, "error while executing Socket::send()");
            return -1;
        }
    }
    return 0;
}

SocketAcceptPollState::SocketAcceptPollState(ExceptionSink* xsink, qore_socket_private* sock, SocketSource* source)
        : sock(sock), source(source) {
}

/** returns:
- SOCK_POLLIN = wait for read and call this again
- SOCK_POLLOUT = wait for write and call this again
- 0 = done
- < 1 = error (exception raised)
*/
int SocketAcceptPollState::continuePoll(ExceptionSink* xsink) {
    // try an accept with no timeout
    int rc = sock->accept_internal(xsink, source);
    if (*xsink) {
        return -1;
    }
    if (rc < 0) {
        return SOCK_POLLIN;
    }
    descriptor = rc;
    return 0;
}

SocketAcceptSslPollState::SocketAcceptSslPollState(ExceptionSink* xsink, qore_socket_private* sock,
        QoreSSLCertificate* cert, QoreSSLPrivateKey* pkey) : sock(sock) {
    assert(sock->sock);
    assert(!sock->ssl);
    SSLSocketHelperHelper sshh(sock, true);

    sock->do_start_ssl_event();
    int rc;
    if ((rc = sock->ssl->setServer(xsink, "acceptSSL", sock->sock, cert, pkey))) {
        sshh.error();
        assert(*xsink);
        return;
    }

    // Set ALPN protocols if configured (for HTTP/2 support)
    if (!sock->alpn_protocols.empty()) {
        sock->ssl->setServerAlpnProtocols(sock->alpn_protocols);
    }
}

int SocketAcceptSslPollState::continuePoll(ExceptionSink* xsink) {
    return sock->ssl->startAccept(xsink);
}

SocketShutdownSslPollState::SocketShutdownSslPollState(ExceptionSink* xsink, qore_socket_private* sock)
        : sock(sock) {
}

int SocketShutdownSslPollState::continuePoll(ExceptionSink* xsink) {
    if (!sock->ssl) {
        return 0;
    }
    return sock->ssl->startShutdown(xsink);
}

SocketShutdownPollOperation::SocketShutdownPollOperation(QoreSocketObject* sock)
        : SocketPollSocketOperationBase(sock) {
}

QoreHashNode* SocketShutdownPollOperation::continuePoll(ExceptionSink* xsink) {
    my_socket_priv* priv = my_socket_priv::getPriv(*sock);
    AutoLocker al(priv->m);

    if (done) {
        return nullptr;
    }

    if (priv->checkValid(xsink)) {
        return nullptr;
    }

    rc = qore_socket_private::get(*priv->socket)->shutdown_direct();
    done = true;
    return nullptr;
}

SocketRecvPacketPollState::SocketRecvPacketPollState(ExceptionSink* xsink, qore_socket_private* sock) : sock(sock),
        bin(new BinaryNode) {
    // first take any data in the socket buffer
    if (sock->buflen) {
        bin->append(sock->rbuf + sock->bufoffset, sock->buflen);
        sock->buflen = 0;
        sock->bufoffset = 0;
        //printd(5, "SocketRecvPacketPollState::SocketRecvPacketPollState() wrote %d bytes of memory from buffer to "
        //    "bin\n", (int)bin->size());
    }
}

/** returns:
    - SOCK_POLLIN = wait for read and call this again
    - SOCK_POLLOUT = wait for write and call this again
    - 0 = done
    - < 0 = error (exception raised)
*/
int SocketRecvPacketPollState::continuePoll(ExceptionSink* xsink) {
    if (io) {
        return 0;
    }

    OptionalNonBlockingHelper nbh(*sock, true, xsink);
    if (*xsink) {
        return -1;
    }

    unsigned loop = 0;

    size_t realsize = bin->size();

    while (true) {
        // ensure space in the binary object to write the socket data
        if (bin->preallocate(realsize + DEFAULT_SOCKET_BUFSIZE)) {
            xsink->outOfMemory();
            return -1;
        }

        ssize_t rc;
        if (sock->ssl) {
            size_t real_io = 0;
            rc = sock->ssl->doNonBlockingIo(xsink, "read",
                reinterpret_cast<void*>(const_cast<char*>(reinterpret_cast<const char*>(bin->getPtr()) + realsize)),
                DEFAULT_SOCKET_BUFSIZE, SslAction::READ, real_io);
            if (*xsink) {
                assert(!real_io);
                bin->setSize(realsize);
                // if we have received data, we must return it
                if (realsize) {
                    xsink->clear();
                    io = true;
                    return 0;
                }
                return -1;
            }
            if (!rc && real_io) {
                if (real_io) {
                    realsize += real_io;
                }
                bin->setSize(realsize);
                // unconditionally done if the socket is closed
                if (!sock->isOpen()) {
                    io = true;
                    return 0;
                }
                // done if we have performed max_nonblock_ops loops
                if (++loop >= max_nonblock_ops) {
                    if (realsize) {
                        io = true;
                        return 0;
                    }
                    return SOCK_POLLIN;
                }
                // otherwise continue reading
                continue;
            }
            bin->setSize(realsize);
            if (realsize) {
                io = true;
                return 0;
            }
            return rc;
        } else {
            //printd(5, "SocketRecvPacketPollState::continuePoll() calling recv\n");
            rc = ::recv(sock->sock,
#ifdef _Q_WINDOWS
                const_cast<char*>(reinterpret_cast<const char*>(bin->getPtr()) + realsize),
#else
                reinterpret_cast<void*>(const_cast<char*>(reinterpret_cast<const char*>(bin->getPtr()) + realsize)),
#endif
                DEFAULT_SOCKET_BUFSIZE, 0);
            //printd(5, "SocketRecvPacketPollState::continuePoll() rc: %d errno: %d bin: %d)\n", rc, errno,
            //    (int)realsize);
            if (rc >= 0) {
                if (rc) {
                    realsize += rc;
                }
                bin->setSize(realsize);
                // done if we have received no data ((rc == 0) => EOF)
                if (!rc) {
                    qore_socket_close_private_from_controller(sock);
                    io = true;
                    return 0;
                }
                // done if we have done max_nonblock_ops loops
                if (++loop >= max_nonblock_ops) {
                    //printd(5, "SocketRecvPacketPollState::continuePoll() DONE bin: %d)\n", (int)bin->size());
                    io = true;
                    return 0;
                }
                // do another recv
                continue;
            }
            bin->setSize(realsize);
            sock_get_error();
            if (errno == EINTR) {
                // do not allow more than max_nonblock_ops loops at a time
                if (++loop >= max_nonblock_ops) {
                    if (realsize) {
                        //printd(5, "SocketRecvPacketPollState::continuePoll() DONE (I/O) bin: %d)\n",
                        //    (int)bin->size());
                        io = true;
                        return 0;
                    }
                    //printd(5, "SocketRecvPacketPollState::continuePoll() waiting\n");
                    return SOCK_POLLIN;
                }
                continue;
            }
            if (realsize) {
                io = true;
                return 0;
            }
            if (errno == EAGAIN
#ifdef EWOULDBLOCK
                || errno == EWOULDBLOCK
#endif
            ) {
                return SOCK_POLLIN;
            }
            xsink->raiseErrnoException("SOCKET-RECV-ERROR", errno, "error while executing Socket::recv()");
            return -1;
        }
    }
    return 0;
}

SocketRecvPollState::SocketRecvPollState(ExceptionSink* xsink, qore_socket_private* sock, size_t size) : sock(sock),
        bin(new BinaryNode), size(size) {
    if (bin->preallocate(size)) {
        xsink->outOfMemory();
        return;
    }
    // first take any data in the socket buffer
    if (sock->buflen) {
        if (sock->buflen <= size) {
            // cannot fail - memory preallocated above
            bin->writeTo(0, sock->rbuf + sock->bufoffset, sock->buflen);
            received = sock->buflen;
            sock->buflen = 0;
            sock->bufoffset = 0;
        } else {
            // cannot fail - memory preallocated above
            bin->writeTo(0, sock->rbuf + sock->bufoffset, size);
            received = size;
            sock->buflen -= size;
            sock->bufoffset += size;
        }
        //printd(5, "SocketRecvPollState::SocketRecvPollState(size: %zu) wrote %d bytes of memory from buffer to "
        //    "bin (remaining %d bytes in buffer)\n", size, (int)received, sock->buflen);
    }
}

/** returns:
    - SOCK_POLLIN = wait for read and call this again
    - SOCK_POLLOUT = wait for write and call this again
    - 0 = done
    - < 0 = error (exception raised)
*/
int SocketRecvPollState::continuePoll(ExceptionSink* xsink) {
    if (received == size) {
        return 0;
    }

    OptionalNonBlockingHelper nbh(*sock, true, xsink);
    if (*xsink) {
        return -1;
    }

    unsigned loop = 0;

    while (true) {
        ssize_t rc;
        if (sock->ssl) {
            size_t real_io = 0;
            rc = sock->ssl->doNonBlockingIo(xsink, "read",
                reinterpret_cast<void*>(const_cast<char*>(reinterpret_cast<const char*>(bin->getPtr()) + received)),
                size - received, SslAction::READ, real_io);
            if (*xsink) {
                return -1;
            }
            if (!rc && real_io) {
                received += real_io;
                if (received == size) {
                    bin->setSize(size);
                    break;
                }
                if (++loop >= max_nonblock_ops) {
                    return SOCK_POLLIN;
                }
                // do another read
                continue;
            }
            // rc == 0 && real_io == 0 is SSL_ERROR_ZERO_RETURN / peer-initiated
            // close-notify (see SSLSocketHelper::doNonBlockingIo handling).
            // Reporting "0 = success" here would let the caller take the
            // output buffer which is only `received` bytes of valid data
            // followed by `size - received` bytes of uninitialized memory
            // preallocated by preallocate(size) — the caller has no way to
            // detect the short read.  Surface it as SOCKET-CLOSED so the H1
            // client body-read gets a real error (matching the non-SSL path
            // below).  Seen in HttpServerAsyncStreamingResponse.qtest
            // testH1ShortStream where the server streams 100 bytes + close
            // with a declared Content-Length of 10000 — the client was
            // reporting a 10000-byte body containing 9900 bytes of
            // uninitialized memory (INVALID-ENCODING on any string decode).
            if (rc == 0 && received < size) {
                xsink->raiseException("SOCKET-CLOSED",
                    "peer closed the SSL connection after " QLLD " of " QLLD
                    " bytes read", (int64)received, (int64)size);
                return -1;
            }
            return rc;
        } else {
            rc = ::recv(sock->sock,
#ifdef _Q_WINDOWS
                const_cast<char*>(reinterpret_cast<const char*>(bin->getPtr()) + received),
#else
                reinterpret_cast<void*>(const_cast<char*>(reinterpret_cast<const char*>(bin->getPtr()) + received)),
#endif
                size - received, 0);
            //printd(5, "SocketRecvPollState::continuePoll() left: %ld rc: %d errno: %d)\n", size - received, rc,
            //    errno);
            if (*xsink) {
                return -1;
            }
            if (rc == 0) {
                // EOF — peer closed the connection before we got the requested size.
                // Returning 0 here would be misinterpreted as "success, done"; returning
                // SOCK_POLLIN would spin in a tight re-poll loop (poll wakes immediately
                // on a closed fd → recv returns 0 → we'd return SOCK_POLLIN again).
                // Report SOCKET-CLOSED so the caller can surface a real error — seen
                // under load as HttpServerAsyncStreamingResponse.qtest Content-Length
                // mismatch producing FUTURE-TIMEOUT instead of SOCKET/HTTP error.
                xsink->raiseException("SOCKET-CLOSED",
                    "peer closed the connection after " QLLD " of " QLLD " bytes read",
                    (int64)received, (int64)size);
                return -1;
            }
            if (rc > 0) {
                received += rc;
                if (received == size) {
                    bin->setSize(size);
                    break;
                }
                // do not allow more than max_nonblock_ops loops at a time
                if (++loop >= max_nonblock_ops) {
                    return SOCK_POLLIN;
                }
                // do another recv
                continue;
            }
            sock_get_error();
            if (errno == EINTR) {
                // do not allow more than max_nonblock_ops loops at a time
                if (++loop >= max_nonblock_ops) {
                    return SOCK_POLLIN;
                }
                continue;
            }
            if (errno == EAGAIN
#ifdef EWOULDBLOCK
                || errno == EWOULDBLOCK
#endif
            ) {
                return SOCK_POLLIN;
            }
            xsink->raiseErrnoException("SOCKET-RECV-ERROR", errno, "error while executing Socket::recv()");
            return -1;
        }
    }
    return 0;
}

SocketRecvSomePollState::SocketRecvSomePollState(ExceptionSink* xsink, qore_socket_private* sock, size_t size)
        : sock(sock), bin(new BinaryNode), size(size) {
    if (!size) {
        io = true;
        return;
    }

    if (sock->buflen) {
        size_t read_size = QORE_MIN(sock->buflen, size);
        bin->append(sock->rbuf + sock->bufoffset, read_size);
        if (sock->buflen == read_size) {
            sock->buflen = 0;
            sock->bufoffset = 0;
        } else {
            sock->buflen -= read_size;
            sock->bufoffset += read_size;
        }
        io = true;
        return;
    }
}

/** returns:
    - SOCK_POLLIN = wait for read and call this again
    - SOCK_POLLOUT = wait for write and call this again
    - 0 = done
    - < 0 = error (exception raised)
*/
int SocketRecvSomePollState::continuePoll(ExceptionSink* xsink) {
    if (io) {
        return 0;
    }

    OptionalNonBlockingHelper nbh(*sock, true, xsink);
    if (*xsink) {
        return -1;
    }

    size_t read_size = QORE_MIN(size, (size_t)DEFAULT_SOCKET_BUFSIZE);
    char buf[DEFAULT_SOCKET_BUFSIZE];

    ssize_t rc;
    if (sock->ssl) {
        size_t real_io = 0;
        rc = sock->ssl->doNonBlockingIo(xsink, "read", buf, read_size, SslAction::READ, real_io);
        if (*xsink) {
            assert(!real_io);
            return -1;
        }
        if (real_io) {
            bin->append(buf, real_io);
            io = true;
            return 0;
        }
        if (!rc && !sock->isOpen()) {
            io = true;
            return 0;
        }
        return rc;
    }

    while (true) {
        rc = ::recv(sock->sock, buf, read_size, 0);
        if (rc > 0) {
            bin->append(buf, rc);
            io = true;
            return 0;
        }
        if (rc == 0) {
            qore_socket_close_private_from_controller(sock);
            io = true;
            return 0;
        }

        sock_get_error();
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN
#ifdef EWOULDBLOCK
            || errno == EWOULDBLOCK
#endif
        ) {
            return SOCK_POLLIN;
        }
        xsink->raiseErrnoException("SOCKET-RECV-ERROR", errno, "error while executing Socket::recv()");
        return -1;
    }
}

SocketRecvUntilBytesPollState::SocketRecvUntilBytesPollState(ExceptionSink* xsink, qore_socket_private* sock,
        const char* bytes, size_t size) : sock(sock), bin(new QoreStringNode), bytes(bytes), size(size) {
}

/** returns:
    - SOCK_POLLIN = wait for read and call this again
    - SOCK_POLLOUT = wait for write and call this again
    - 0 = done
    - < 0 = error (exception raised)
*/
int SocketRecvUntilBytesPollState::continuePoll(ExceptionSink* xsink) {
    if (matched == size) {
        return 0;
    }

    while (true) {
        char c;
        if (sock->readByteFromBuffer(c)) {
            int rc = doRecv(xsink);
            ASYNC_IO_TRACE("RecvUntilBytes: doRecv rc=%d buflen=%zd bufoffset=%zd matched=%zu/%zu ssl=%d\n",
                rc, sock->buflen, sock->bufoffset, matched, size, sock->ssl ? 1 : 0);
            if (!rc) {
                if (!sock->buflen) {
                    if (bin->empty()) {
                        se_closed("Socket", "recvUntilBytes", xsink);
                    } else {
                        xsink->raiseException("SOCKET-HTTP-ERROR", "remote end closed connection while reading "
                            "HTTP data");
                    }
                    return -1;
                }
                continue;
            }
            if (*xsink) {
                return -1;
            }
            return rc;
        }

        bin->concat(c);
        if (c == bytes[matched]) {
            ++matched;
            if (matched == size) {
                break;
            }
        } else if (matched) {
            matched = 0;
        }
    }

    return 0;
}

int SocketRecvUntilBytesPollState::doRecv(ExceptionSink* xsink) {
    OptionalNonBlockingHelper nbh(*sock, true, xsink);
    if (*xsink) {
        return -1;
    }

    unsigned loop = 0;

    while (true) {
        ssize_t rc;
        if (sock->ssl) {
            size_t real_io = 0;
            rc = sock->ssl->doNonBlockingIo(xsink, "read", sock->rbuf, DEFAULT_SOCKET_BUFSIZE, SslAction::READ,
                real_io);
            if (*xsink) {
                return -1;
            }
            if (!rc) {
                assert(!sock->bufoffset);
                sock->buflen = real_io;
            }
            if (rc == SOCK_POLLIN && !real_io) {
                if (++loop >= max_nonblock_ops) {
                    return SOCK_POLLIN;
                }
                short revents;
                int prc = qore_socket_poll_fd_revents_now(sock->sock, POLLIN | POLLERR | POLLHUP, revents,
                    "SOCKET-RECV-ERROR", "socket", xsink);
                if (prc < 0) {
                    return -1;
                }
                if (!prc || !(revents & (POLLIN | POLLERR | POLLHUP))) {
                    return SOCK_POLLIN;
                }
                if (revents & (POLLERR | POLLHUP)) {
                    return 0;
                }
                continue;
            }
            assert(!rc || rc == 1 || rc == 2 || rc == 3 || rc == -1);
            return rc;
        } else {
            rc = ::recv(sock->sock, sock->rbuf, DEFAULT_SOCKET_BUFSIZE, 0);
            if (!rc) {
                return 0;
            }
            if (rc > 0) {
                assert(!sock->bufoffset);
                sock->buflen = rc;
                break;
            }
            sock_get_error();
            if (errno == EINTR) {
                // do not allow more than max_nonblock_ops loops at a time
                if (++loop >= max_nonblock_ops) {
                    return SOCK_POLLIN;
                }
                continue;
            }
            if (errno == EAGAIN
#ifdef EWOULDBLOCK
                || errno == EWOULDBLOCK
#endif
            ) {
                return SOCK_POLLIN;
            }
            xsink->raiseErrnoException("SOCKET-RECV-ERROR", errno, "error while executing Socket::recv()");
            return -1;
        }
    }
    return 0;
}

SocketRecvFromPollState::SocketRecvFromPollState(ExceptionSink* xsink, qore_socket_private* sock, size_t max_size)
        : sock(sock), max_size(max_size) {
    if (sock->sock == QORE_INVALID_SOCKET) {
        xsink->raiseException("SOCKET-NOT-OPEN", "socket is not open");
        return;
    }
    if (sock->stype != SOCK_DGRAM) {
        xsink->raiseException("SOCKET-RECVFROM-ERROR", "recvfrom() is only valid for UDP (SOCK_DGRAM) sockets");
        return;
    }
    bin = new BinaryNode();
    if (bin->preallocate(max_size)) {
        xsink->outOfMemory();
        return;
    }
    memset(&src_addr, 0, sizeof(src_addr));
}

// NOTE: Unlike TCP poll states that start by returning SOCK_POLLIN and do I/O on the second call,
// this may return 0 (completed) on the first call if a datagram is already available.
// This is correct because UDP recvfrom() is atomic — one call = one datagram, no partial reads.
int SocketRecvFromPollState::continuePoll(ExceptionSink* xsink) {
    if (io) {
        return 0;
    }

    OptionalNonBlockingHelper nbh(*sock, true, xsink);
    if (*xsink) {
        return -1;
    }

    unsigned loop = 0;

    while (true) {
        src_addr_len = sizeof(src_addr);
        ssize_t rc = ::recvfrom(sock->sock,
#ifdef _Q_WINDOWS
            const_cast<char*>(reinterpret_cast<const char*>(bin->getPtr())),
#else
            reinterpret_cast<void*>(const_cast<char*>(reinterpret_cast<const char*>(bin->getPtr()))),
#endif
            max_size, 0, (struct sockaddr*)&src_addr, &src_addr_len);

        if (rc >= 0) {
            bin->setSize(rc);
            received = rc;
            io = true;
            buildOutput(xsink);
            return *xsink ? -1 : 0;
        }

        sock_get_error();
        if (errno == EINTR) {
            if (++loop >= max_nonblock_ops) {
                return SOCK_POLLIN;
            }
            continue;
        }
        if (errno == EAGAIN
#ifdef EWOULDBLOCK
            || errno == EWOULDBLOCK
#endif
        ) {
            return SOCK_POLLIN;
        }
        xsink->raiseErrnoException("SOCKET-RECVFROM-ERROR", errno, "error while executing recvfrom()");
        return -1;
    }
}

QoreValue SocketRecvFromPollState::takeOutput() {
    if (!output) {
        return QoreValue();
    }
    QoreHashNode* rv = output;
    output = nullptr;
    return rv;
}

void SocketRecvFromPollState::buildOutput(ExceptionSink* xsink) {
    assert(!output);
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclDatagramInfo, xsink), xsink);
    if (*xsink) {
        return;
    }

    // Set the binary data
    h->setKeyValue("data", bin.release(), xsink);

    // Extract source address info
    if (src_addr.ss_family == AF_INET || src_addr.ss_family == AF_INET6) {
        char ifname[INET6_ADDRSTRLEN];
        if (inet_ntop(src_addr.ss_family, qore_get_in_addr((struct sockaddr*)&src_addr),
                ifname, sizeof(ifname))) {
            h->setKeyValue("address", new QoreStringNode(ifname), xsink);
        }

        int port;
        if (src_addr.ss_family == AF_INET) {
            struct sockaddr_in* s = (struct sockaddr_in*)&src_addr;
            port = ntohs(s->sin_port);
        } else {
            struct sockaddr_in6* s = (struct sockaddr_in6*)&src_addr;
            port = ntohs(s->sin6_port);
        }
        h->setKeyValue("port", port, xsink);
    }

    h->setKeyValue("family", (int64)src_addr.ss_family, xsink);
    h->setKeyValue("familystr", new QoreStringNode(QoreAddrInfo::getFamilyName(src_addr.ss_family)), xsink);

    if (!*xsink) {
        output = h.release();
    }
}

SocketSendToPollState::SocketSendToPollState(ExceptionSink* xsink, qore_socket_private* sock, BinaryNode* bin,
        const struct sockaddr* dest_addr, socklen_t dest_addr_len)
        : sock(sock), bin(bin), data(reinterpret_cast<const char*>(bin->getPtr())), size(bin->size()),
        dest_addr_len(dest_addr_len) {
    if (sock->sock == QORE_INVALID_SOCKET) {
        xsink->raiseException("SOCKET-NOT-OPEN", "socket is not open");
        return;
    }
    if (sock->stype != SOCK_DGRAM) {
        xsink->raiseException("SOCKET-SENDTO-ERROR", "sendto() is only valid for UDP (SOCK_DGRAM) sockets");
        return;
    }
    memcpy(&this->dest_addr, dest_addr, dest_addr_len);
}

int SocketSendToPollState::continuePoll(ExceptionSink* xsink) {
    OptionalNonBlockingHelper nbh(*sock, true, xsink);
    if (*xsink) {
        return -1;
    }

    unsigned loop = 0;

    while (true) {
        ssize_t rc = ::sendto(sock->sock, data + sent, size - sent, 0,
            (const struct sockaddr*)&dest_addr, dest_addr_len);

        if (rc >= 0) {
            sent += rc;
            if (sent == size) {
                return 0;
            }
            // For UDP, partial sends are unusual but handle them
            if (++loop >= max_nonblock_ops) {
                return SOCK_POLLOUT;
            }
            continue;
        }

        sock_get_error();
        if (errno == EINTR) {
            if (++loop >= max_nonblock_ops) {
                return SOCK_POLLOUT;
            }
            continue;
        }
        if (errno == EAGAIN
#ifdef EWOULDBLOCK
            || errno == EWOULDBLOCK
#endif
        ) {
            return SOCK_POLLOUT;
        }
        xsink->raiseErrnoException("SOCKET-SENDTO-ERROR", errno, "error while executing sendto()");
        return -1;
    }
    return 0;
}

void qore_socket_private::captureRemoteCert(X509_STORE_CTX* x509_ctx) {
    assert(x509_ctx);
    //printd(5, "qore_socket_private::captureRemoteCert() x509_ctx: %p current_sock: %p\n", x509_ctx, current_socket);
    if (!current_socket) {
        return;
    }

    X509* x509 = X509_STORE_CTX_get_current_cert(x509_ctx);
    assert(x509);
    // issue #3665: deref any current client cert before assigning
    if (current_socket->remote_cert) {
        current_socket->remote_cert->deref(nullptr);
    }
    current_socket->remote_cert = new QoreObject(QC_SSLCERTIFICATE, getProgram(),
        new QoreSSLCertificate(X509_dup(x509)));
}

QoreListNode* qore_socket_private::poll(const QoreListNode* poll_list, int timeout_ms, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(hashdeclSocketPollInfo->getTypeInfo(false)), xsink);

    if (poll_list->empty()) {
        return rv.release();
    }

    SocketSyncPoll::assertNotOnIoThread("Socket", "poll", xsink);
    if (*xsink) {
        return nullptr;
    }

    struct PollEntry {
        size_t index;
        int64 events;
        QoreObject* original_obj;
        QoreObject* poll_obj;
        bool original_is_pollable;
        struct PollTarget {
            QoreObject* poll_obj;
            int events;
        };
        std::vector<PollTarget> extra_targets;
    };

    struct PollObjectCleanup {
        std::vector<QoreObject*> objects;

        ~PollObjectCleanup() {
            for (QoreObject* obj : objects) {
                ExceptionSink cleanup_xsink;
                obj->deref(&cleanup_xsink);
                cleanup_xsink.clear();
            }
        }
    };

    std::vector<PollEntry> entries;
    PollObjectCleanup cleanup{{}};
    int64 poll_timeout_hint_ms = -1;

    ConstListIterator li(poll_list);
    unsigned cancel_check = 0;
    while (li.next()) {
        if (!(cancel_check++ % 100) && qore_check_cancel(xsink, "Socket::poll setup")) {
            return nullptr;
        }

        const QoreValue v = li.getValue();
        assert(QoreTypeInfo::getUniqueReturnHashDecl(v.getFullTypeInfo())->equal(hashdeclSocketPollInfo));
        const QoreHashNode* h = v.get<const QoreHashNode>();
        assert(h);
        bool found;
        int64 events = h->getKeyAsBigInt("events", found);
        QoreValue poll_timeout_value = h->getKeyValue("poll_timeout_ms");
        if (!poll_timeout_value.isNullOrNothing()) {
            int64 poll_timeout_ms = poll_timeout_value.getAsBigInt();
            if (poll_timeout_hint_ms < 0 || poll_timeout_ms < poll_timeout_hint_ms) {
                poll_timeout_hint_ms = poll_timeout_ms;
            }
        }

        // get the socket
        QoreObject* obj;
        {
            QoreValue v = h->getKeyValue("socket");
            if (v.getType() != NT_OBJECT) {
                assert(v.isNothing());
                xsink->raiseException("SOCKET-POLL-ERROR", "element %zu/%zu (starting from 1) is missing "
                    "the 'socket' value", li.index() + 1, poll_list->size());
                return nullptr;
            }
            obj = v.get<QoreObject>();
        }

        QoreObject* poll_obj = nullptr;
        // first see if the object inherits AbstractPollableIoObjectBase
        TryPrivateDataRefHolder<AbstractPollableIoObjectBase> io(obj, CID_ABSTRACTPOLLABLEIOOBJECTBASE, xsink);
        if (*xsink) {
            return nullptr;
        }
        // if so, get the descriptor; this is faster than executing a %Qore method
        int fd;
        if (io) {
            fd = io->getPollableDescriptor();
            poll_obj = obj;
        } else {
            fd = obj->evalMethod("getPollableDescriptor", nullptr, xsink).getAsBigInt();
            if (*xsink) {
                return nullptr;
            }
            poll_obj = new QoreObject(QC_ABSTRACTPOLLABLEIOOBJECTBASE, getProgram(),
                new QoreSocketPollListPollable(fd));
            cleanup.objects.push_back(poll_obj);
        }
        if (fd < 0) {
            xsink->raiseException("SOCKET-NOT-OPEN", "element %zu/%zu (starting from 1) references a " \
                "pollable object that is not open", li.index() + 1, poll_list->size());
            return nullptr;
        }

        std::vector<PollEntry::PollTarget> extra_targets;
        QoreValue extra_value = h->getKeyValue("extra_fds");
        if (extra_value.getType() == NT_LIST) {
            const QoreListNode* extra_list = extra_value.get<const QoreListNode>();
            ConstListIterator extra_li(extra_list);
            while (extra_li.next()) {
                if (!(cancel_check++ % 100) && qore_check_cancel(xsink, "Socket::poll setup")) {
                    return nullptr;
                }

                QoreValue extra_entry = extra_li.getValue();
                const QoreHashNode* extra_hash = extra_entry.get<const QoreHashNode>();
                assert(extra_hash);

                bool fd_found;
                int64 fd64 = extra_hash->getKeyAsBigInt("fd", fd_found);
                if (!fd_found || fd64 < 0 || fd64 > std::numeric_limits<int>::max()) {
                    xsink->raiseException("SOCKET-POLL-ERROR",
                        "element %zu/%zu (starting from 1) has an invalid extra_fds[%zu].fd value",
                        li.index() + 1, poll_list->size(), extra_li.index());
                    return nullptr;
                }

                bool extra_events_found;
                int64 extra_events64 = extra_hash->getKeyAsBigInt("events", extra_events_found);
                int extra_events = extra_events_found ? static_cast<int>(extra_events64) : 0;
                if (!extra_events) {
                    extra_events = SOCK_POLLIN;
                }
                if (!(extra_events & (SOCK_POLLIN | SOCK_POLLOUT))) {
                    xsink->raiseException("SOCKET-POLL-ERROR",
                        "element %zu/%zu (starting from 1) has an invalid extra_fds[%zu].events value; "
                        "neither SOCK_POLLIN nor SOCK_POLLOUT is set",
                        li.index() + 1, poll_list->size(), extra_li.index());
                    return nullptr;
                }

                std::unique_ptr<QoreSocketPollListPollable> extra_pollable(
                    new QoreSocketPollListPollable(static_cast<int>(fd64)));
                ReferenceHolder<QoreObject> extra_obj(new QoreObject(QC_ABSTRACTPOLLABLEIOOBJECTBASE, getProgram(),
                    extra_pollable.get()), xsink);
                extra_pollable.release();
                QoreObject* extra_poll_obj = extra_obj.release();
                cleanup.objects.push_back(extra_poll_obj);
                extra_targets.push_back({extra_poll_obj, extra_events});
            }
        }

        if (!(events & (SOCK_POLLIN | SOCK_POLLOUT)) && extra_targets.empty()) {
            xsink->raiseException("SOCKET-POLL-ERROR", "element %zu/%zu (starting from 1) has an invalid " \
                "'events' value; neither SOCK_POLLIN nor SOCK_POLLOUT is set", li.index() + 1, poll_list->size());
            return nullptr;
        }

        entries.push_back({li.index(), events, obj, poll_obj, static_cast<bool>(io), std::move(extra_targets)});
    }

    std::map<size_t, int> result_events;

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

    ReferenceHolder<QoreObject> queue_obj(new QoreObject(QC_QUEUE, getProgram(), new Queue()), xsink);
    ReferenceHolder<Queue> queue(
        static_cast<Queue*>((*queue_obj)->getReferencedPrivateData(CID_QUEUE, xsink)), xsink);
    if (*xsink || !queue) {
        return nullptr;
    }

    static std::atomic<uint64_t> poll_seq{0};
    uint64_t seq = poll_seq.fetch_add(1, std::memory_order_relaxed);
    QoreStringMaker owner("Socket::poll:%" PRIu64, seq);
    std::shared_ptr<std::atomic<bool>> armed = std::make_shared<std::atomic<bool>>(false);

    auto cancel_owner = [&]() {
        ExceptionSink cleanup_xsink;
        SimpleRefHolder<QoreStringNode> owner_str(new QoreStringNode(owner.c_str()));
        ctrl->cancelByOwner(*owner_str, &cleanup_xsink);
        cleanup_xsink.clear();
    };

    size_t submitted = 0;
    cancel_check = 0;
    uint64_t target_seq = 0;

    auto submit_readiness_poll = [&](const PollEntry& entry, QoreObject* poll_obj, int event) -> int {
        if (!(cancel_check++ % 100) && qore_check_cancel(xsink, "Socket::poll submit")) {
            cancel_owner();
            return -1;
        }

        ReferenceHolder<SocketPollOperationBase> poller(
            new QoreSocketPollListReadinessPollOperation(poll_obj, event, armed), xsink);
        ReferenceHolder<QoreObject> op_obj(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), *poller), xsink);
        poller->setSelf(*op_obj);
        poller.release();
        if (!*xsink) {
            op_obj->setValue("sock", poll_obj->objectRefSelf(), xsink);
            op_obj->setValue("goal", new QoreStringNode("poll"), xsink);
        }
        if (*xsink) {
            cancel_owner();
            return -1;
        }

        ReferenceHolder<QoreHashNode> other(new QoreHashNode(autoTypeInfo), xsink);
        other->setKeyValue("index", static_cast<int64>(entry.index), xsink);
        other->setKeyValue("events", event, xsink);
        if (*xsink) {
            cancel_owner();
            return -1;
        }

        uint64_t target = target_seq++;
        QoreStringMaker key("Socket::poll:%" PRIu64 ":%zu:%" PRIu64 ":%d", seq, entry.index, target, event);
        ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollOperationInfo, xsink), xsink);
        info->setKeyValue("sock", poll_obj->objectRefSelf(), xsink);
        info->setKeyValue("spop", (*op_obj)->objectRefSelf(), xsink);
        info->setKeyValue("owner", new QoreStringNode(owner.c_str()), xsink);
        info->setKeyValue("key", new QoreStringNode(key.c_str()), xsink);
        info->setKeyValue("thread_key", new QoreStringNode(owner.c_str()), xsink);
        info->setKeyValue("to", -1, xsink);
        info->setKeyValue("resultQueue", (*queue_obj)->objectRefSelf(), xsink);
        info->setKeyValue("other", other.release(), xsink);
        if (*xsink) {
            cancel_owner();
            return -1;
        }

        ReferenceHolder<QoreObject> submit_rv(ctrl->submit(*ctl_obj, info.release(), false, xsink), xsink);
        if (*xsink) {
            cancel_owner();
            return -1;
        }
        ++submitted;
        return 0;
    };

    for (const PollEntry& entry : entries) {
        for (int event : {SOCK_POLLIN, SOCK_POLLOUT}) {
            if (!(entry.events & event)) {
                continue;
            }

            if (submit_readiness_poll(entry, entry.poll_obj, event)) {
                return nullptr;
            }
        }

        for (const PollEntry::PollTarget& target : entry.extra_targets) {
            for (int event : {SOCK_POLLIN, SOCK_POLLOUT}) {
                if (!(target.events & event)) {
                    continue;
                }
                if (submit_readiness_poll(entry, target.poll_obj, event)) {
                    return nullptr;
                }
            }
        }
    }

    if (submitted && !ctrl->waitForProcessing(owner.c_str(), 0, xsink)) {
        cancel_owner();
        xsink->raiseException("SOCKET-POLL-ERROR",
            "async I/O controller stopped before Socket::poll() operations were processed");
        return nullptr;
    }
    armed->store(true, std::memory_order_release);

    std::map<size_t, std::pair<QoreObject*, bool>> entry_objects;
    for (const PollEntry& entry : entries) {
        entry_objects[entry.index] = {entry.original_obj, entry.original_is_pollable};
    }

    auto entry_closed = [&](size_t index) -> bool {
        auto i = entry_objects.find(index);
        if (i == entry_objects.end()) {
            return false;
        }
        if (!i->second.second) {
            return false;
        }

        QoreObject* obj = i->second.first;
        if (!obj->isCurrent()) {
            return true;
        }

        ExceptionSink cleanup_xsink;
        AbstractPollableIoObjectBase* io = static_cast<AbstractPollableIoObjectBase*>(
            obj->getReferencedPrivateData(CID_ABSTRACTPOLLABLEIOOBJECTBASE, &cleanup_xsink));
        if (cleanup_xsink) {
            cleanup_xsink.clear();
            return true;
        }
        if (!io) {
            return false;
        }
        ReferenceHolder<AbstractPollableIoObjectBase> holder(io, &cleanup_xsink);
        return io->getPollableDescriptor() < 0;
    };

    auto consume_result = [&](const QoreHashNode* result) -> int {
        QoreValue ex = result->getKeyValue("ex");
        if (ex.getType() == NT_HASH) {
            const QoreHashNode* ex_hash = ex.get<const QoreHashNode>();
            if (qore_socket_exec_exception_is(*ex_hash, "SOCKET-TIMEOUT")) {
                return 0;
            }
            qore_socket_raise_poll_result_exception(ex_hash, xsink);
            return -1;
        }

        QoreValue other = result->getKeyValue("other");
        if (other.getType() != NT_HASH) {
            return 0;
        }
        const QoreHashNode* other_hash = other.get<const QoreHashNode>();
        size_t index = (size_t)other_hash->getKeyValue("index").getAsBigInt();
        int events = (int)other_hash->getKeyValue("events").getAsBigInt();
        QoreValue canceled = result->getKeyValue("canceled");
        if (canceled.getAsBool()) {
            if (entry_closed(index)) {
                result_events[index] |= SOCK_POLLERR;
            }
            return 0;
        }
        result_events[index] |= events;
        return 0;
    };

    auto shift_one = [&](int qtimeout) -> int {
        bool timed_out = false;
        ValueHolder result(queue->shift(xsink, qtimeout, &timed_out), xsink);
        if (*xsink) {
            return -1;
        }
        if (timed_out) {
            return 1;
        }
        if (result->getType() != NT_HASH) {
            xsink->raiseException("SOCKET-POLL-ERROR",
                "expected SocketPollResultInfo hash from async poll operation, got '%s'",
                result->getFullTypeName());
            return -1;
        }
        return consume_result(result->get<const QoreHashNode>());
    };

    auto mark_closed_entries = [&]() -> int {
        unsigned close_check = 0;
        for (const PollEntry& entry : entries) {
            if (!(close_check++ % 100) && qore_check_cancel(xsink, "Socket::poll result build")) {
                return -1;
            }
            if (entry_closed(entry.index)) {
                result_events[entry.index] = SOCK_POLLERR;
            }
        }
        return 0;
    };

    auto build_results = [&]() -> QoreListNode* {
        if (mark_closed_entries()) {
            return nullptr;
        }

        unsigned result_check = 0;
        for (const auto& [idx, events] : result_events) {
            if (!(result_check++ % 100) && qore_check_cancel(xsink, "Socket::poll result build")) {
                return nullptr;
            }
            if (!events) {
                continue;
            }
            const QoreHashNode* orig = poll_list->retrieveEntry(idx).get<const QoreHashNode>();
            ReferenceHolder<QoreHashNode> entry(new QoreHashNode(hashdeclSocketPollInfo, xsink), xsink);
            entry->setKeyValue("events", events, xsink);
            entry->setKeyValue("socket", orig->getKeyValue("socket").refSelf(), xsink);
            rv->push(entry.release(), xsink);
            if (*xsink) {
                return nullptr;
            }
        }

        return rv.release();
    };

    if (submitted) {
        int64 effective_timeout_ms = timeout_ms;
        if (poll_timeout_hint_ms >= 0
                && (effective_timeout_ms < 0 || poll_timeout_hint_ms < effective_timeout_ms)) {
            effective_timeout_ms = poll_timeout_hint_ms;
        }
        int queue_timeout = effective_timeout_ms < 0
            ? 0
            : (effective_timeout_ms == 0 ? -1 : static_cast<int>(std::min<int64>(effective_timeout_ms,
                std::numeric_limits<int>::max())));
        int rc = shift_one(queue_timeout);
        if (rc < 0) {
            cancel_owner();
            return nullptr;
        }
        if (rc > 0) {
            cancel_owner();
            while (true) {
                rc = shift_one(-1);
                if (rc < 0) {
                    return nullptr;
                }
                if (rc > 0) {
                    break;
                }
            }
            return build_results();
        }
    }

    cancel_owner();

    while (true) {
        int rc = shift_one(-1);
        if (rc < 0) {
            return nullptr;
        }
        if (rc > 0) {
            break;
        }
    }

    return build_results();
}

static void write_sse_key_value(ExceptionSink* xsink, ReferenceHolder<QoreHashNode>& rv, const char* f, QoreValue v,
        SimpleRefHolder<QoreStringNode>& value) {
    assert(value && !value->empty());
    if (!v) {
        rv->setKeyValue(f, value.release(), xsink);
    } else {
        QoreStringNodeValueHelper old_value(v);
        QoreStringNode* str = new QoreStringNode(**old_value);
        str->concat('\n');
        str->concat(*value, xsink);
        rv->setKeyValue(f, str, xsink);
        value->clear();
    }
}

static void write_sse_key_value(ExceptionSink* xsink, ReferenceHolder<QoreHashNode>& rv, const char* f,
        SimpleRefHolder<QoreStringNode>& value) {
    write_sse_key_value(xsink, rv, f, rv->getKeyValue(f), value);
}

static void write_sse_key(ExceptionSink* xsink, ReferenceHolder<QoreHashNode>& rv, const char* f,
        SimpleRefHolder<QoreStringNode>& value) {
    QoreValue v = rv->getKeyValue(f);
    if (!value || value->empty()) {
        // if the field does not exist, set it to an empty string
        if (!v) {
            rv->setKeyValue(f, new QoreStringNode(QCS_UTF8), xsink);
        }
    } else {
        write_sse_key_value(xsink, rv, f, v, value);
    }
}

static bool sse_is_digits(const char* str) {
    if (!str[0]) {
        return false;
    }
    for (const char* p = str; *p; ++p) {
        if (!isdigit(*p)) {
            return false;
        }
    }
    return true;
}

// Standalone SSE event parser — reused by SseAction on the I/O thread
QoreHashNode* parseSseEvent(ExceptionSink* xsink, const QoreString& buf) {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclSseMessageInfo, xsink), xsink);
    assert(!*xsink);
    SimpleRefHolder<QoreStringNode> field;
    SimpleRefHolder<QoreStringNode> value;
    int do_value = 0;
    for (size_t i = 0, e = buf.size() - 1; i < e; ++i) {
        char c = buf[i];
        if (c == '\n') {
            if (field && !field->empty()) {
                if ((**field == "event") || (**field == "data")) {
                    write_sse_key(xsink, rv, field->c_str(), value);
                } else if (**field == "id") {
                    if (value && !value->empty()) {
                        write_sse_key_value(xsink, rv, "id", value);
                    }
                } else if (**field == "retry") {
                    if (value && !value->empty() && sse_is_digits(value->c_str()) && !rv->getKeyValue("retry")) {
                        rv->setKeyValue("retry", strtoll(value->c_str(), 0, 10), xsink);
                    }
                }
                // ignore field data
                field->clear();
            }
            if (value && !value->empty()) {
                write_sse_key_value(xsink, rv, "comment", value);
            }
            if (do_value) {
                do_value = 0;
            }
            continue;
        }
        if (!do_value && (c == ':')) {
            do_value = 1;
            continue;
        }
        if (do_value) {
            if (!value) {
                value = new QoreStringNode(QCS_UTF8);
            }
            // skip the first space after the colon
            if (do_value == 1) {
                do_value = 2;
                if (c == ' ') {
                    continue;
                }
            }
            value->concat(c);
            continue;
        }
        if (!field) {
            field = new QoreStringNode(QCS_UTF8);
        }
        field->concat(c);
    }

    //QoreNodeAsStringHelper str(*rv, FMT_NORMAL, xsink);
    //printd(0, "qore_socket_private::parseServerSentEvent() %s\n", str->c_str());

    return rv.release();
}

// --- SseAction implementation ---

void SseAction::execute(QoreValue output, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lg(mtx);

    // Extract body data from the streaming data hash.  Intermediate H2/H3
    // chunks are binary, but an H2 DATA+END_STREAM completion can pass through
    // the regular completed-response path where text/event-stream is decoded
    // to a Qore string before the action sees it.
    if (output.getType() == NT_HASH) {
        QoreValue body_val = output.get<QoreHashNode>()->getKeyValue("body");
        if (body_val.getType() == NT_BINARY) {
            const BinaryNode* data = body_val.get<const BinaryNode>();
            if (data->size() > 0) {
                sse_buffer.concat((const char*)data->getPtr(), data->size());
            }
        } else if (body_val.getType() == NT_STRING) {
            QoreStringValueHelper data(body_val);
            if (!data->empty()) {
                sse_buffer.concat(data->c_str(), data->size());
            }
        }
    }
    output.discard(xsink);

    bool pushed = false;

    // Parse complete SSE events (terminated by double newline)
    while (true) {
        // Look for \n\n or \r\n\r\n boundary
        qore_offset_t pos = sse_buffer.find("\n\n");
        size_t sep_len = 2;
        if (pos < 0) {
            pos = sse_buffer.find("\r\n\r\n");
            sep_len = 4;
            if (pos < 0) {
                break;
            }
        }
        // Extract the event text (without the terminator)
        QoreString event_text(sse_buffer.c_str(), pos);
        event_text.concat("\n\n");  // parseSseEvent loop uses (i < size-1), needs double \n
        // Remove consumed bytes from buffer
        sse_buffer.replace(0, pos + sep_len, "");
        QoreHashNode* evt = parseSseEvent(xsink, event_text);
        if (evt && queue) {
            queue->pushAndTakeRef(evt);
            pushed = true;
        }
    }

    if (pushed) {
        notify();
    }
}

void QoreSocket::doException(int rc, const char* meth, int timeout_ms, ExceptionSink* xsink) {
    assert(xsink);
    switch (rc) {
        case 0:
            se_closed("Socket", meth, xsink);
            break;
        case QSE_RECV_ERR: // recv() error
            xsink->raiseException("SOCKET-RECV-ERROR", q_strerror(errno));
            break;
        case QSE_NOT_OPEN:
            se_not_open("Socket", meth, xsink);
            break;
        case QSE_TIMEOUT:
            se_timeout("Socket", meth, timeout_ms, xsink);
            break;
        case QSE_SSL_ERR:
            xsink->raiseException("SOCKET-SSL-ERROR", "SSL error in Socket::%s() call", meth);
            break;
        case QSE_IN_OP:
            se_in_op("Socket", meth, xsink);
            break;
        case QSE_IN_OP_THREAD:
            se_in_op_thread("Socket", meth, xsink);
            break;
        default:
            xsink->raiseException("SOCKET-ERROR", "unknown internal error code %d in Socket::%s() call", rc, meth);
            break;
    }
}

#ifndef HAVE_SSL_READ_EX
DLLLOCAL int SSL_read_ex(SSL* ssl, void* buf, size_t num, size_t* readbytes) {
    (*readbytes) = 0;
    int rc = SSL_read(ssl, buf, num);
    if (rc > 0) {
        (*readbytes) = rc;
        rc = 1;
    } else {
        rc = 0;
    }
    return rc;
}

DLLLOCAL int SSL_peek_ex(SSL* ssl, void* buf, size_t num, size_t* readbytes) {
    (*readbytes) = 0;
    int rc = SSL_peek(ssl, buf, num);
    if (rc > 0) {
        (*readbytes) = rc;
        rc = 1;
    } else {
        rc = 0;
    }
    return rc;
}

DLLLOCAL int SSL_write_ex(SSL* ssl, const void* buf, size_t num, size_t* written) {
    (*written) = 0;
    int rc = SSL_write(ssl, buf, num);
    if (rc > 0) {
        (*written) = rc;
        rc = 1;
    } else {
        rc = 0;
    }
    return rc;
}
#endif

/**
    we assume the socket has already been put in a nonblock state

    returns:
    - SOCK_POLLIN = wait for read and call this again
    - SOCK_POLLOUT = wait for write and call this again
    - 0 = done
    - < 0 = error (exception raised)
*/
int SSLSocketHelper::doNonBlockingIo(ExceptionSink* xsink, const char* mname, void* buf, size_t size,
        SslAction action, size_t& real_io) {
    assert(xsink);
    assert(size);
    assert(!real_io);
    SSLSocketReferenceHelper ssrh(this);

    int rc;
    while (true) {
        ERR_clear_error();
        switch (action) {
            case READ:
                rc = SSL_read_ex(ssl, buf, size, &real_io);
                break;
            case WRITE:
                rc = SSL_write_ex(ssl, buf, size, &real_io);
                break;
            case PEEK:
                rc = SSL_peek_ex(ssl, buf, size, &real_io);
                break;
        }

        if (rc > 0) {
#ifdef DEBUG
            // Test-only hook to simulate SSL partial writes that require repeated polling.
            static int force_count = 0;
            static int force_max = 0;
            const char* env = getenv("QORE_TEST_SSL_PARTIAL_WRITE");
            if (!env || !*env || *env == '0') {
                force_count = 0;
                force_max = 0;
            } else if (action == WRITE && real_io) {
                if (!force_max) {
                    const char* lim = getenv("QORE_TEST_SSL_PARTIAL_WRITE_LIMIT");
                    force_max = lim ? atoi(lim) : 4;
                    if (force_max < 1) {
                        force_max = 1;
                    }
                }
                if (force_count < force_max) {
                    ++force_count;
                    rc = SOCK_POLLOUT;
                    break;
                }
            }
#endif
            // SSL accepted the data, but check if it still has pending encrypted data to write
            // This happens when SSL_write encrypts data but the socket would block
            if (action == WRITE && SSL_want_write(ssl)) {
                rc = SOCK_POLLOUT;
                break;
            }
            rc = 0;
            break;
        }

        int err = SSL_get_error(ssl, rc);

        //printd(5, "SSLSocketHelper::doNonBlockingIo() %s size: %ld action: %s err: %d\n", mname, size,
        //    get_action_method(action), err);

        if (err == SSL_ERROR_WANT_READ) {
            rc = SOCK_POLLIN;
            break;
        } else if (err == SSL_ERROR_WANT_WRITE) {
            rc = SOCK_POLLOUT;
            break;
        } else if (err == SSL_ERROR_ZERO_RETURN) {
            // here we allow the remote side to disconnect and return 0 the first time just like regular recv()
            if (action != WRITE) {
                rc = 0;
            } else {
                if (!sslError(xsink, mname, "SSL_write"))
                    xsink->raiseException("SOCKET-SSL-ERROR", "error in Socket::%s(): the socket was closed by the "
                        "remote host while calling SSL_write()", mname);
                rc = QSE_SSL_ERR;
            }
            // close the local socket unconditionally
            // For HTTP/2, let the HTTP/2 layer handle connection lifecycle
            if (!qs.h2_session) {
                qore_socket_close_private_from_controller(&qs);
            }
            break;
        } else if (err == SSL_ERROR_SYSCALL) {
            if (!sslError(xsink, mname, get_action_method(action), action == WRITE)) {
                if (!rc) {
                    xsink->raiseException("SOCKET-SSL-ERROR", "error in Socket::%s(): the openssl library reported " \
                        "an EOF condition that violates the SSL protocol while calling %s()", mname,
                        get_action_method(action));
                } else if (rc == -1) {
                    xsink->raiseErrnoException("SOCKET-SSL-ERROR", sock_get_error(), "error in Socket::%s(): the " \
                        "openssl library reported an I/O error while calling %s()", mname, get_action_method(action));
                } else {
                    xsink->raiseException("SOCKET-SSL-ERROR", "error in Socket::%s(): the openssl library reported " \
                        "error code %d in %s() but the error queue is empty", mname, rc, get_action_method(action));
                }
            }
            // close the local socket unconditionally
            // For HTTP/2, let the HTTP/2 layer handle connection lifecycle
            if (!qs.h2_session) {
                qore_socket_close_private_from_controller(&qs);
            }
            // in case there is no exception when reading, the remote end closed the connection
            rc = *xsink ? QSE_SSL_ERR : 0;
            break;
        } else if (err == SSL_ERROR_SSL) {
            // must call sslError() before closing; close_internal() derefs ssl, which would
            // bring refs down to 1, violating sslError()'s assert(refs > 1) invariant
            if (!sslError(xsink, mname, get_action_method(action), action == WRITE)) {
                xsink->raiseErrnoException("SOCKET-SSL-ERROR", sock_get_error(), "error in Socket::%s(): the " \
                    "openssl library reported a fatal I/O error while calling %s()", mname, get_action_method(action));
            }
            // For HTTP/2, let the HTTP/2 layer handle connection lifecycle
            if (!qs.h2_session) {
                qore_socket_close_private_from_controller(&qs);
            }
            rc = QSE_SSL_ERR;
            break;
        } else {
            //printd(5, "SSLSocketHelper::doNonBlockingIo(buf: %p size: %ld) rc: %d err: %d\n", buf, size, rc, err);
            // always throw an exception if an error occurs while writing
            if (!sslError(xsink, mname, get_action_method(action), action == WRITE)) {
                rc = 0;
            }
            break;
        }
    }

    //printd(5, "SSLSocketHelper::doNonBlockingIo(buf: %p size: %d action: %d) rc: %d\n", buf, size, action, rc);
    return rc;
}

DLLLOCAL OptionalNonBlockingHelper::OptionalNonBlockingHelper(qore_socket_private& s, bool n_set, ExceptionSink* xs)
        : sock(s), xsink(xs), set(false) {
    if (n_set) {
        if (qore_on_async_io_thread()) {
            // On the I/O thread: ensure the socket is non-blocking but do NOT
            // restore blocking in the destructor — the socket must stay
            // non-blocking for the duration of its time on the I/O thread.
            sock.set_non_blocking(true, xs);
            // 'set' stays false — destructor will not restore blocking
        } else {
            if (!sock.set_non_blocking(true, xs)) {
                set = true;
            }
        }
    }
}

DLLLOCAL OptionalNonBlockingHelper::~OptionalNonBlockingHelper() {
    if (set) {
        sock.set_non_blocking(false, xsink);
    }
}

// returns true if an error was raised or the connection was closed, false if not
bool SSLSocketHelper::sslError(ExceptionSink* xsink, const char* mname, const char* func, bool always_error) {
    assert(refs > 1);
    assert(xsink);

    long e = ERR_get_error();
    do {
        //printd(5, "SSLSocketHelper::sslError() '%s' func: '%s' always_error: %d e: %ld\n", mname, func, always_error, e);
        handleErrorIntern(xsink, e ? e : SSL_ERROR_ZERO_RETURN, mname, func, always_error);
    } while ((e = ERR_get_error()));

    return *xsink || !qs.isOpen();
}

void SSLSocketHelper::handleErrorIntern(ExceptionSink* xsink, int e, const char* mname, const char* func,
        bool always_error) {
    if (e == SSL_ERROR_ZERO_RETURN) {
        // the remote end has closed the connection
        // NOTE: For HTTP/2 connections, don't auto-close on SSL_ERROR_ZERO_RETURN.
        // This could be a timeout or the end of a read operation, not necessarily a connection close.
        // Let the HTTP/2 layer handle connection lifecycle.
        if (!qs.h2_session) {
            qore_socket_close_private_from_controller(&qs);
        }
        if (always_error) {
            xsink->raiseException("SOCKET-SSL-ERROR", "error in Socket::%s(): the %s() call could not be " \
                "completed because the TLS/SSL connection was terminated (err: %d)", mname, func, e);
        }
    } else {
        char buf[121];
        ERR_error_string(e, buf);
        SimpleRefHolder<QoreStringNode> errstr(new QoreStringNodeMaker("error in Socket::%s(): %s(): %s", mname,
            func, buf));
        // issue #3818: consume any ssl_err_str remaining
        if (qs.ssl_err_str) {
            errstr->concat(": ");
            qore_string_private::get(*errstr)->concat(qs.ssl_err_str);
            qs.ssl_err_str->deref();
            qs.ssl_err_str = nullptr;
        }
        xsink->raiseException("SOCKET-SSL-ERROR", errstr.release());
#ifdef ECONNRESET
        // close the socket if connection reset received
        if (e == SSL_ERROR_SYSCALL && sock_get_error() == ECONNRESET) {
            //printd(5, "SSLSocketHelper::handleErrorIntern() Socket::%s() (%s) socket closed by remote end\n", mname, func);
            // For HTTP/2 connections, let the HTTP/2 layer handle connection lifecycle
            if (!qs.h2_session) {
                qore_socket_close_private_from_controller(&qs);
            }
        }
#endif
    }
}

PrivateQoreSocketTimeoutHelper::PrivateQoreSocketTimeoutHelper(qore_socket_private* s, const char* o)
        : PrivateQoreSocketTimeoutBase(s && s->tl_warning_us.load(std::memory_order_relaxed) ? s : nullptr), op(o) {
}

PrivateQoreSocketTimeoutHelper::~PrivateQoreSocketTimeoutHelper() {
    if (!sock)
        return;

    int64 dt = q_clock_getmicros() - start;
    if (dt >= sock->tl_warning_us.load(std::memory_order_relaxed)) {
        sock->doTimeoutWarning(op, dt);
    }
}

PrivateQoreSocketThroughputHelper::PrivateQoreSocketThroughputHelper(qore_socket_private* s, bool snd)
        : PrivateQoreSocketTimeoutBase(s), send(snd) {
}

PrivateQoreSocketThroughputHelper::~PrivateQoreSocketThroughputHelper() {
}

void PrivateQoreSocketThroughputHelper::finalize(int64 bytes) {
    //printd(5, "PrivateQoreSocketThroughputHelper::finalize() bytes: " QLLD " us: " QLLD " (min: " QLLD ") bs: %.6f "
    //    "threshold: %.6f\n", bytes, (q_clock_getmicros() - start), sock->tp_us_min, ((double)bytes /
    //    ((double)(q_clock_getmicros() - start) / (double)1000000.0)), sock->tp_warning_bs);

    if (bytes < DEFAULT_SOCKET_MIN_THRESHOLD_BYTES) {
        return;
    }

    int64 dt = q_clock_getmicros() - start;
    if (send) {
        sock->tp_bytes_sent.fetch_add(bytes, std::memory_order_relaxed);
        sock->tp_us_sent.fetch_add(dt, std::memory_order_relaxed);
    } else {
        sock->tp_bytes_recv.fetch_add(bytes, std::memory_order_relaxed);
        sock->tp_us_recv.fetch_add(dt, std::memory_order_relaxed);
    }

    double warning_bs = sock->tp_warning_bs.load(std::memory_order_relaxed);
    if (!warning_bs) {
        return;
    }

    // ignore if less than event time threshold
    if (dt < sock->tp_us_min.load(std::memory_order_relaxed)) {
        return;
    }

    double bs = (double)bytes / ((double)dt / (double)1000000.0);

    //printd(5, "PrivateQoreSocketThroughputHelper::finalize() bytes: " QLLD " us: " QLLD " bs: %.6f threshold: "
    //    %.6f\n", bytes, dt, bs, sock->tp_warning_bs);

    if (bs <= warning_bs) {
        sock->doThroughputWarning(send, bytes, dt, bs);
    }
}

QoreSocket::QoreSocket() : priv(new qore_socket_private) {
}

QoreSocket::QoreSocket(int n_sock, int n_sfamily, int n_stype, int n_prot, const QoreEncoding* n_enc)
        : priv(new qore_socket_private(n_sock, n_sfamily, n_stype, n_prot, n_enc)) {
}

QoreSocket::~QoreSocket() {
    qore_socket_exec_close(this);
    priv->deref();
}

int QoreSocket::setNoDelay(int nodelay) {
    return qore_socket_exec_setup_no_exception(this,
        new QoreSocketControllerSetupPollOperation(this,
            QoreSocketControllerSetupPollOperation::ConfigAction::SetNoDelay, nodelay),
        "setNoDelay");
}

int QoreSocket::getNoDelay() const {
    return qore_socket_exec_setup_no_exception(const_cast<QoreSocket*>(this),
        new QoreSocketControllerSetupPollOperation(const_cast<QoreSocket*>(this),
            QoreSocketControllerSetupPollOperation::ConfigAction::GetNoDelay),
        "getNoDelay");
}

int QoreSocket::setUserTimeout(int ms) {
    return qore_socket_exec_setup_no_exception(this,
        new QoreSocketControllerSetupPollOperation(this,
            QoreSocketControllerSetupPollOperation::ConfigAction::SetUserTimeout, ms),
        "setUserTimeout");
}

int QoreSocket::getUserTimeout() const {
    return qore_socket_exec_setup_no_exception(const_cast<QoreSocket*>(this),
        new QoreSocketControllerSetupPollOperation(const_cast<QoreSocket*>(this),
            QoreSocketControllerSetupPollOperation::ConfigAction::GetUserTimeout),
        "getUserTimeout");
}

int QoreSocket::close() {
    return qore_socket_exec_close(this);
}

int QoreSocket::shutdown() {
    return qore_socket_exec_setup_no_exception(this, new QoreSocketControllerSetupPollOperation(this), "shutdown");
}

int QoreSocket::shutdownSSL(ExceptionSink* xsink) {
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_ALL);
    if (!io_guard) {
        return -1;
    }

    ValueHolder rv(qore_socket_exec_poll(this,
        new QoreSocketControllerSslShutdownPollOperation(this), -1, "shutdownSSL", "ssl-shutdown", xsink),
        xsink);
    return *xsink ? -1 : 0;
}

int QoreSocket::getSocket() const {
    return priv->sock;
}

const QoreEncoding* QoreSocket::getEncoding() const {
    return priv->enc;
}

void QoreSocket::setEncoding(const QoreEncoding* id) {
    priv->enc = id;
}

bool QoreSocket::isOpen() const {
    return (bool)(priv->sock != QORE_INVALID_SOCKET);
}

const char* QoreSocket::getSSLCipherName() const {
    if (qore_on_async_io_thread()) {
        return priv->ssl ? priv->ssl->getCipherName() : nullptr;
    }

    ExceptionSink xsink;
    SimpleRefHolder<QoreStringNode> str(qore_socket_exec_tls_state_string(const_cast<QoreSocket*>(this),
        QoreSocketControllerTlsStatePollOperation::Action::GetCipherName, "getSSLCipherName", &xsink));
    AutoLocker al(priv->tls_state_cache_m);
    if (xsink || !str) {
        xsink.clear();
        priv->ssl_cipher_name_cache.clear();
        return nullptr;
    }
    priv->ssl_cipher_name_cache = str->c_str();
    return priv->ssl_cipher_name_cache.c_str();
}

const char* QoreSocket::getSSLCipherVersion() const {
    if (qore_on_async_io_thread()) {
        return priv->ssl ? priv->ssl->getCipherVersion() : nullptr;
    }

    ExceptionSink xsink;
    SimpleRefHolder<QoreStringNode> str(qore_socket_exec_tls_state_string(const_cast<QoreSocket*>(this),
        QoreSocketControllerTlsStatePollOperation::Action::GetCipherVersion, "getSSLCipherVersion", &xsink));
    AutoLocker al(priv->tls_state_cache_m);
    if (xsink || !str) {
        xsink.clear();
        priv->ssl_cipher_version_cache.clear();
        return nullptr;
    }
    priv->ssl_cipher_version_cache = str->c_str();
    return priv->ssl_cipher_version_cache.c_str();
}

bool QoreSocket::isSecure() const {
    if (qore_on_async_io_thread()) {
        return (bool)priv->ssl;
    }

    ExceptionSink xsink;
    bool rv = qore_socket_exec_tls_state_bool(const_cast<QoreSocket*>(this),
        QoreSocketControllerTlsStatePollOperation::Action::IsSecure, "isSecure", &xsink);
    if (xsink) {
        xsink.clear();
        return false;
    }
    return rv;
}

int QoreSocket::setAlpnProtocols(const QoreListNode* protocols, ExceptionSink* xsink) {
    std::vector<std::string> proto_list;
    if (my_socket_priv::parseAlpnProtocols(protocols, proto_list, xsink)) {
        return -1;
    }

    if (qore_on_async_io_thread()) {
        priv->alpn_protocols = std::move(proto_list);
        return 0;
    }

    return qore_socket_exec_tls_state_no_output(this,
        new QoreSocketControllerTlsStatePollOperation(this, std::move(proto_list)), "setAlpnProtocols", xsink);
}

void QoreSocket::clearAlpnProtocols() {
    if (qore_on_async_io_thread()) {
        priv->alpn_protocols.clear();
        return;
    }

    ExceptionSink xsink;
    qore_socket_exec_tls_state_no_output(this,
        new QoreSocketControllerTlsStatePollOperation(this,
            QoreSocketControllerTlsStatePollOperation::Action::ClearAlpnProtocols),
        "clearAlpnProtocols", &xsink);
    if (xsink) {
        xsink.clear();
    }
}

QoreStringNode* QoreSocket::getAlpnProtocol() const {
    if (qore_on_async_io_thread()) {
        if (!priv->ssl) {
            return nullptr;
        }
        std::string proto = priv->ssl->getAlpnProtocol();
        return proto.empty() ? nullptr : new QoreStringNode(proto.c_str());
    }

    ExceptionSink xsink;
    QoreStringNode* rv = qore_socket_exec_tls_state_string(const_cast<QoreSocket*>(this),
        QoreSocketControllerTlsStatePollOperation::Action::GetAlpnProtocol, "getAlpnProtocol", &xsink);
    if (xsink) {
        xsink.clear();
        return nullptr;
    }
    return rv;
}

bool QoreSocket::isHttp2() const {
    if (qore_on_async_io_thread()) {
        return priv->ssl ? priv->ssl->isHttp2() : false;
    }

    ExceptionSink xsink;
    bool rv = qore_socket_exec_tls_state_bool(const_cast<QoreSocket*>(this),
        QoreSocketControllerTlsStatePollOperation::Action::IsHttp2, "isHttp2", &xsink);
    if (xsink) {
        xsink.clear();
        return false;
    }
    return rv;
}

int32_t QoreSocket::submitHttp2PushPromise(int32_t stream_id, const char* path,
        const QoreHashNode* headers, ExceptionSink* xsink) {
    return static_cast<int32_t>(qore_socket_exec_http2_enqueue_int(this,
        new QoreSocketControllerHttp2EnqueuePollOperation(this,
            QoreSocketControllerHttp2EnqueuePollOperation::Action::PushPromise, stream_id, path, headers),
        "submitHttp2PushPromise", xsink));
}

int QoreSocket::submitHttp2Response(int32_t stream_id, int status_code,
        const QoreHashNode* headers, const void* body, size_t body_len,
        ExceptionSink* xsink) {
    return static_cast<int>(qore_socket_exec_http2_enqueue_int(this,
        new QoreSocketControllerHttp2EnqueuePollOperation(this,
            QoreSocketControllerHttp2EnqueuePollOperation::Action::Response, stream_id, status_code, headers, body,
            body_len),
        "submitHttp2Response", xsink));
}

int QoreSocket::submitHttp2ConnectResponse(int32_t stream_id, int status_code,
        const QoreHashNode* headers, ExceptionSink* xsink) {
    return static_cast<int>(qore_socket_exec_http2_enqueue_int(this,
        new QoreSocketControllerHttp2EnqueuePollOperation(this,
            QoreSocketControllerHttp2EnqueuePollOperation::Action::ConnectResponse, stream_id, status_code, headers),
        "submitHttp2ConnectResponse", xsink));
}

int32_t QoreSocket::submitHttp2Request(const QoreHashNode* headers, const void* body,
        size_t body_len, ExceptionSink* xsink, bool streaming) {
    QoreSocketControllerHttp2EnqueuePollOperation* poller =
        new QoreSocketControllerHttp2EnqueuePollOperation(this, headers, body, body_len, streaming, xsink);
    if (*xsink) {
        poller->deref(xsink);
        return -1;
    }
    return static_cast<int32_t>(qore_socket_exec_http2_enqueue_int(this, poller, "submitHttp2Request", xsink));
}

void QoreSocket::cancelHttp2Stream(int32_t stream_id, ExceptionSink* xsink) {
    qore_socket_exec_http2_enqueue_int(this,
        new QoreSocketControllerHttp2EnqueuePollOperation(this,
            QoreSocketControllerHttp2EnqueuePollOperation::Action::Cancel, stream_id),
        "cancelHttp2Stream", xsink);
}

void QoreSocket::setHttp2ConnectProtocolEnabled(bool enable) {
    priv->h2_enable_connect_protocol = enable;
}

int QoreSocket::sendHttp2StreamData(int32_t stream_id, const BinaryNode* data,
        bool end_stream, ExceptionSink* xsink) {
    return static_cast<int>(qore_socket_exec_http2_enqueue_int(this,
        new QoreSocketControllerHttp2EnqueuePollOperation(this, stream_id, data, end_stream),
        "sendHttp2StreamData", xsink));
}

int QoreSocket::sendHttp2Trailers(int32_t stream_id, const QoreHashNode* trailers,
        ExceptionSink* xsink) {
    return static_cast<int>(qore_socket_exec_http2_enqueue_int(this,
        new QoreSocketControllerHttp2EnqueuePollOperation(this,
            QoreSocketControllerHttp2EnqueuePollOperation::Action::Trailers, stream_id, 0, trailers),
        "sendHttp2Trailers", xsink));
}

BinaryNode* QoreSocket::readHttp2StreamData(int32_t stream_id, size_t max_bytes, ExceptionSink* xsink) {
    return qore_socket_exec_http2_stream_data(this,
        new QoreSocketControllerHttp2StreamDataPollOperation(this, stream_id, max_bytes), stream_id, xsink);
}

bool QoreSocket::isHttp2StreamClosed(int32_t stream_id) const {
    ExceptionSink xsink;
    bool rv = qore_socket_exec_http2_stream_state(const_cast<QoreSocket*>(this),
        new QoreSocketControllerHttp2StreamStatePollOperation(const_cast<QoreSocket*>(this), stream_id,
            QoreSocketControllerHttp2StreamStatePollOperation::Action::Closed),
        "isHttp2StreamClosed", &xsink);
    if (xsink) {
        xsink.clear();
        return true;
    }
    return rv;
}

bool QoreSocket::isHttp2StreamRemoteClosed(int32_t stream_id) const {
    ExceptionSink xsink;
    bool rv = qore_socket_exec_http2_stream_state(const_cast<QoreSocket*>(this),
        new QoreSocketControllerHttp2StreamStatePollOperation(const_cast<QoreSocket*>(this), stream_id,
            QoreSocketControllerHttp2StreamStatePollOperation::Action::RemoteClosed),
        "isHttp2StreamRemoteClosed", &xsink);
    if (xsink) {
        xsink.clear();
        return true;
    }
    return rv;
}

int QoreSocket::waitForHttp2StreamDrain(int32_t stream_id, int timeout_ms) {
    ExceptionSink xsink;
    int rc = waitForHttp2StreamDrain(stream_id, timeout_ms, &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

int QoreSocket::waitForHttp2StreamDrain(int32_t stream_id, int timeout_ms, ExceptionSink* xsink) {
    SocketSyncPoll::assertNotOnIoThread("Socket", "waitForHttp2StreamDrain", xsink);
    if (*xsink) {
        return -1;
    }
    QoreHashNode* ex = nullptr;
    std::string owner_name("waitForHttp2StreamDrain:");
    owner_name += std::to_string(stream_id);
    ValueHolder result(qore_socket_exec_poll(this,
        new QoreSocketHttp2StreamDrainPollOperation(this, stream_id), timeout_ms,
        owner_name.c_str(), "stream-drained", xsink, &ex), xsink);
    ReferenceHolder<QoreHashNode> ex_holder(ex, xsink);
    if (*xsink) {
        return -1;
    }
    if (ex_holder) {
        if (qore_socket_exec_exception_is(**ex_holder, "SOCKET-TIMEOUT")) {
            return 1;
        }
        qore_socket_raise_poll_result_exception(*ex_holder, xsink);
        return -1;
    }
    return result->isNothing() ? -1 : static_cast<int>(result->getAsBigInt());
}

long QoreSocket::verifyPeerCertificate() const {
    if (qore_on_async_io_thread()) {
        return priv->ssl ? priv->ssl->verifyPeerCertificate() : -1;
    }

    ExceptionSink xsink;
    int64 rv = qore_socket_exec_tls_state_int(const_cast<QoreSocket*>(this),
        QoreSocketControllerTlsStatePollOperation::Action::VerifyPeerCertificate, "verifyPeerCertificate", &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return static_cast<long>(rv);
}

// hardcoded to SOCK_STREAM (tcp only)
int QoreSocket::connectINET(const char* host, int prt, int timeout_ms, ExceptionSink* xsink) {
    QoreString service;
    service.sprintf("%d", prt);

    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_ALL);
    if (!io_guard) {
        return -1;
    }

    ValueHolder rv(qore_socket_exec_poll(this,
        new QoreSocketControllerConnectPollOperation(this, host, service.c_str(), AF_UNSPEC, SOCK_STREAM, 0),
        timeout_ms, "connectINET", "connected", xsink), xsink);
    return *xsink ? -1 : 0;
}

int QoreSocket::connectINET(const char* host, int prt, ExceptionSink* xsink) {
    QoreString service;
    service.sprintf("%d", prt);

    return connectINET(host, prt, -1, xsink);
}

int QoreSocket::connectINET2(const char* name, const char* service, int family, int socktype, int protocol,
        int timeout_ms, ExceptionSink* xsink) {
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_ALL);
    if (!io_guard) {
        return -1;
    }

    ValueHolder rv(qore_socket_exec_poll(this,
        new QoreSocketControllerConnectPollOperation(this, name, service, family, socktype, protocol),
        timeout_ms, "connectINET2", "connected", xsink), xsink);
    return *xsink ? -1 : 0;
}

int QoreSocket::connectUNIX(const char* p, ExceptionSink* xsink) {
    return connectUNIX(p, SOCK_STREAM, 0, xsink);
}

int QoreSocket::connectUNIX(const char* p, int sock_type, int protocol, ExceptionSink* xsink) {
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_ALL);
    if (!io_guard) {
        return -1;
    }

    ValueHolder rv(qore_socket_exec_poll(this,
        new QoreSocketControllerConnectPollOperation(this, p, sock_type, protocol),
        -1, "connectUNIX", "connected", xsink), xsink);
    return *xsink ? -1 : 0;
}

// currently hardcoded to SOCK_STREAM (tcp-only)
// starts a connection to a remote socket
// for AF_INET sockets:
// * QoreSocket::startConnect("hostname:<port_number>");
// for AF_UNIX sockets:
// * QoreSocket::startConnect("filename");
AbstractPollState* QoreSocket::startConnect(ExceptionSink* xsink, const char* name) {
    const char* p;

    if ((p = strrchr(name, ':'))) {
        QoreString host(name, p - name);
        QoreString service(p + 1);
        // if the address is an ipv6 address like: [<addr>], then connect as ipv6
        if (host.strlen() > 2 && host[0] == '[' && host[host.strlen() - 1] == ']') {
            host.terminate(host.strlen() - 1);
            //printd(5, "QoreSocket::connect(%s, %s) [ipv6]\n", host.c_str() + 1, service.c_str());
            // Explicit IPv6 bracket notation — single family, no racing needed
            return new SocketConnectInetHappyEyeballsPollState(xsink, priv, host.c_str() + 1, service.c_str(),
                AF_INET6);
        }
        return new SocketConnectInetHappyEyeballsPollState(xsink, priv, host.c_str(), service.c_str());
    }

    // otherwise assume it's a file name for a UNIX domain socket
#ifndef _Q_WINDOWS
    return new SocketConnectUnixPollState(xsink, priv, name);
#else
    missing_function_error("Socket::startConnect(<UNIX socket file>)", "UNIX_FILEMGT", xsink);
    return nullptr;
#endif
}

AbstractPollState* QoreSocket::startConnectINET(ExceptionSink* xsink, const char* host, const char* service,
        int family, int socktype, int protocol) {
    return new SocketConnectInetHappyEyeballsPollState(xsink, priv, host, service, family, socktype, protocol);
}

AbstractPollState* QoreSocket::startConnectUNIX(ExceptionSink* xsink, const char* path, int socktype,
        int protocol) {
#ifndef _Q_WINDOWS
    return new SocketConnectUnixPollState(xsink, priv, path, socktype, protocol);
#else
    missing_function_error("Socket::startConnectUNIX()", "UNIX_FILEMGT", xsink);
    return nullptr;
#endif
}

AbstractPollState* QoreSocket::startSslConnect(ExceptionSink* xsink, QoreSSLCertificate* cert,
        QoreSSLPrivateKey* pkey) {
    if (priv->sock == QORE_INVALID_SOCKET) {
        se_not_open("Socket", "startSslConnect", xsink);
        return nullptr;
    }
    if (priv->ssl) {
        se_ssl_already_established("Socket", "startSslConnect", xsink);
        return nullptr;
    }
    return new SocketConnectSslPollState(xsink, priv, cert, pkey);
}

AbstractPollState* QoreSocket::startSend(ExceptionSink* xsink, const char* data, size_t size) {
    if (priv->sock == QORE_INVALID_SOCKET) {
        se_not_open("Socket", "startSend", xsink);
        return nullptr;
    }

    return new SocketSendPollState(xsink, priv, data, size);
}

AbstractPollState* QoreSocket::startRecv(ExceptionSink* xsink, size_t size) {
    if (priv->sock == QORE_INVALID_SOCKET) {
        se_not_open("Socket", "startRecv", xsink);
        return nullptr;
    }

    return new SocketRecvPollState(xsink, priv, size);
}

AbstractPollState* QoreSocket::startRecvSome(ExceptionSink* xsink, size_t size) {
    if (priv->sock == QORE_INVALID_SOCKET) {
        se_not_open("Socket", "startRecvSome", xsink);
        return nullptr;
    }

    return new SocketRecvSomePollState(xsink, priv, size);
}

AbstractPollState* QoreSocket::startRecvUntilBytes(ExceptionSink* xsink, const char* pattern, size_t size) {
    if (priv->sock == QORE_INVALID_SOCKET) {
        se_not_open("Socket", "startRecvUntilBytes", xsink);
        return nullptr;
    }

    return new SocketRecvUntilBytesPollState(xsink, priv, pattern, size);
}

AbstractPollState* QoreSocket::startRecvPacket(ExceptionSink* xsink) {
    if (priv->sock == QORE_INVALID_SOCKET) {
        se_not_open("Socket", "startRecvPacket", xsink);
        return nullptr;
    }

    return new SocketRecvPacketPollState(xsink, priv);
}

AbstractPollState* QoreSocket::startRecvFrom(ExceptionSink* xsink, size_t max_size) {
    if (priv->sock == QORE_INVALID_SOCKET) {
        se_not_open("Socket", "startRecvFrom", xsink);
        return nullptr;
    }

    return new SocketRecvFromPollState(xsink, priv, max_size);
}

AbstractPollState* QoreSocket::startSendTo(ExceptionSink* xsink, BinaryNode* bin,
        const struct sockaddr* dest_addr, socklen_t dest_addr_len) {
    if (priv->sock == QORE_INVALID_SOCKET) {
        se_not_open("Socket", "startSendTo", xsink);
        return nullptr;
    }

    return new SocketSendToPollState(xsink, priv, bin, dest_addr, dest_addr_len);
}

AbstractPollState* QoreSocket::startAccept(ExceptionSink* xsink) {
    if (priv->sock == QORE_INVALID_SOCKET) {
        se_not_open("Socket", "startAccept", xsink);
        return nullptr;
    }
    return new SocketAcceptPollState(xsink, priv);
}

AbstractPollState* QoreSocket::startSslAccept(ExceptionSink* xsink, QoreSSLCertificate* cert,
        QoreSSLPrivateKey* pkey) {
    if (priv->sock == QORE_INVALID_SOCKET) {
        se_not_open("Socket", "startSslAccept", xsink);
        return nullptr;
    }
    if (priv->ssl) {
        se_ssl_already_established("Socket", "startSslAccept", xsink);
        return nullptr;
    }
    return new SocketAcceptSslPollState(xsink, priv, cert, pkey);
}

// currently hardcoded to SOCK_STREAM (tcp-only)
// opens and connects to a remote socket
// for AF_INET sockets:
// * QoreSocket::connect("hostname:<port_number>");
// for AF_UNIX sockets:
// * QoreSocket::connect("filename");
int QoreSocket::connect(const char* name, int timeout_ms, ExceptionSink* xsink) {
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_ALL);
    if (!io_guard) {
        return -1;
    }

    ValueHolder rv(qore_socket_exec_poll(this,
        new QoreSocketControllerConnectPollOperation(this, name), timeout_ms, "connect", "connected", xsink),
        xsink);
    return *xsink ? -1 : 0;
}

int QoreSocket::connect(const char* name, ExceptionSink* xsink) {
   return connect(name, -1, xsink);
}

// currently hardcoded to SOCK_STREAM (tcp-only)
// opens and connects to a remote socket and negotiates an SSL connection
// for AF_INET sockets:
// * QoreSocket::connectSSL("hostname:<port_number>");
// for AF_UNIX sockets:
// * QoreSocket::connectSSL("filename");
int QoreSocket::connectSSL(ExceptionSink* xsink, const char* name, int timeout_ms, QoreSSLCertificate* cert,
        QoreSSLPrivateKey* pkey) {
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_ALL);
    if (!io_guard) {
        return -1;
    }

    ValueHolder rv(qore_socket_exec_poll(this,
        new QoreSocketControllerConnectPollOperation(this, name, true, cert, pkey),
        timeout_ms, "connectSSL", "ssl-connected", xsink), xsink);
    return *xsink ? -1 : 0;
}

int QoreSocket::connectSSL(ExceptionSink* xsink, const char* name, QoreSSLCertificate* cert,
        QoreSSLPrivateKey* pkey) {
   return connectSSL(xsink, name, -1, cert, pkey);
}

int QoreSocket::connectINETSSL(ExceptionSink* xsink, const char* host, int prt, int timeout_ms,
        QoreSSLCertificate* cert, QoreSSLPrivateKey* pkey) {
    QoreString service;
    service.sprintf("%d", prt);

    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_ALL);
    if (!io_guard) {
        return -1;
    }

    ValueHolder rv(qore_socket_exec_poll(this,
        new QoreSocketControllerConnectPollOperation(this, host, service.c_str(), AF_UNSPEC, SOCK_STREAM, 0,
            true, cert, pkey),
        timeout_ms, "connectINETSSL", "ssl-connected", xsink), xsink);
    return *xsink ? -1 : 0;
}

int QoreSocket::connectINETSSL(ExceptionSink* xsink, const char* host, int prt, QoreSSLCertificate* cert,
        QoreSSLPrivateKey* pkey) {
   return connectINETSSL(xsink, host, prt, -1, cert, pkey);
}

int QoreSocket::connectINET2SSL(ExceptionSink* xsink, const char* name, const char* service, int family,
        int sock_type, int protocol, int timeout_ms, QoreSSLCertificate* cert, QoreSSLPrivateKey* pkey) {
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_ALL);
    if (!io_guard) {
        return -1;
    }

    ValueHolder rv(qore_socket_exec_poll(this,
        new QoreSocketControllerConnectPollOperation(this, name, service, family, sock_type, protocol,
            true, cert, pkey),
        timeout_ms, "connectINET2SSL", "ssl-connected", xsink), xsink);
    return *xsink ? -1 : 0;
}

int QoreSocket::connectUNIXSSL(ExceptionSink* xsink, const char* p, int sock_type, int protocol,
        QoreSSLCertificate* cert, QoreSSLPrivateKey* pkey) {
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_ALL);
    if (!io_guard) {
        return -1;
    }

    ValueHolder rv(qore_socket_exec_poll(this,
        new QoreSocketControllerConnectPollOperation(this, p, sock_type, protocol, true, cert, pkey),
        -1, "connectUNIXSSL", "ssl-connected", xsink), xsink);
    return *xsink ? -1 : 0;
}

int QoreSocket::sendi1(char i) {
    return qore_socket_exec_send_bytes_no_exception(this, &i, 1, -1);
}

int QoreSocket::sendi2(short i) {
    // convert to network byte order
    i = htons(i);
    return qore_socket_exec_send_bytes_no_exception(this, &i, 2, -1);
}

int QoreSocket::sendi4(int i) {
    // convert to network byte order
    i = htonl(i);
    return qore_socket_exec_send_bytes_no_exception(this, &i, 4, -1);
}

int QoreSocket::sendi8(int64 i) {
    // convert to network byte order
    i = i8MSB(i);
    return qore_socket_exec_send_bytes_no_exception(this, &i, 8, -1);
}

int QoreSocket::sendi2LSB(short i) {
    // convert to LSB byte order
    i = i2LSB(i);
    return qore_socket_exec_send_bytes_no_exception(this, &i, 2, -1);
}

int QoreSocket::sendi4LSB(int i) {
    // convert to LSB byte order
    i = i4LSB(i);
    return qore_socket_exec_send_bytes_no_exception(this, &i, 4, -1);
}

int QoreSocket::sendi8LSB(int64 i) {
    // convert to LSB byte order
    i = i8LSB(i);
    return qore_socket_exec_send_bytes_no_exception(this, &i, 8, -1);
}

int QoreSocket::sendi1(char i, int timeout_ms, ExceptionSink* xsink) {
    return qore_socket_exec_send_bytes(this, &i, 1, timeout_ms, xsink);
}

int QoreSocket::sendi2(short i, int timeout_ms, ExceptionSink* xsink) {
    // convert to network byte order
    i = htons(i);
    return qore_socket_exec_send_bytes(this, &i, 2, timeout_ms, xsink);
}

int QoreSocket::sendi4(int i, int timeout_ms, ExceptionSink* xsink) {
    // convert to network byte order
    i = htonl(i);
    return qore_socket_exec_send_bytes(this, &i, 4, timeout_ms, xsink);
}

int QoreSocket::sendi8(int64 i, int timeout_ms, ExceptionSink* xsink) {
    // convert to network byte order
    i = i8MSB(i);
    return qore_socket_exec_send_bytes(this, &i, 8, timeout_ms, xsink);
}

int QoreSocket::sendi2LSB(short i, int timeout_ms, ExceptionSink* xsink) {
    // convert to LSB byte order
    i = i2LSB(i);
    return qore_socket_exec_send_bytes(this, &i, 2, timeout_ms, xsink);
}

int QoreSocket::sendi4LSB(int i, int timeout_ms, ExceptionSink* xsink) {
    // convert to LSB byte order
    i = i4LSB(i);
    return qore_socket_exec_send_bytes(this, &i, 4, timeout_ms, xsink);
}

int QoreSocket::sendi8LSB(int64 i, int timeout_ms, ExceptionSink* xsink) {
    // convert to LSB byte order
    i = i8LSB(i);
    return qore_socket_exec_send_bytes(this, &i, 8, timeout_ms, xsink);
}

// receive integer values and convert from network byte order
int QoreSocket::recvi1(int timeout, char* val) {
    ExceptionSink xsink;
    int rc = qore_socket_exec_recv_integer(this, "recvi1", 1, val, timeout, &xsink);
    // ignore exception; we just use a return code
    if (xsink)
        xsink.clear();
    return rc;
}

int QoreSocket::recvi2(int timeout, short *val) {
   ExceptionSink xsink;
   int rc = qore_socket_exec_recv_integer(this, "recvi2", 2, val, timeout, &xsink);
   if (rc > 0)
      *val = ntohs(*val);
   // ignore exception; we just use a return code
   if (xsink)
      xsink.clear();
   return rc;
}

int QoreSocket::recvi4(int timeout, int* val) {
   ExceptionSink xsink;
   int rc = qore_socket_exec_recv_integer(this, "recvi4", 4, val, timeout, &xsink);
   if (rc > 0)
      *val = ntohl(*val);
   // ignore exception; we just use a return code
   if (xsink)
      xsink.clear();
   return rc;
}

int QoreSocket::recvi8(int timeout, int64 *val) {
   ExceptionSink xsink;
   int rc = qore_socket_exec_recv_integer(this, "recvi8", 8, val, timeout, &xsink);
   if (rc > 0)
      *val = MSBi8(*val);
   // ignore exception; we just use a return code
   if (xsink)
      xsink.clear();
   return rc;
}

int QoreSocket::recvi2LSB(int timeout, short *val) {
   ExceptionSink xsink;
   int rc = qore_socket_exec_recv_integer(this, "recvi2LSB", 2, val, timeout, &xsink);
   if (rc > 0)
      *val = LSBi2(*val);
   // ignore exception; we just use a return code
   if (xsink)
      xsink.clear();
   return rc;
}

int QoreSocket::recvi4LSB(int timeout, int* val) {
   ExceptionSink xsink;
   int rc = qore_socket_exec_recv_integer(this, "recvi4LSB", 4, val, timeout, &xsink);
   if (rc > 0)
      *val = LSBi4(*val);
   // ignore exception; we just use a return code
   if (xsink)
      xsink.clear();
   return rc;
}

int QoreSocket::recvi8LSB(int timeout, int64 *val) {
   ExceptionSink xsink;
   int rc = qore_socket_exec_recv_integer(this, "recvi8LSB", 8, val, timeout, &xsink);
   if (rc > 0)
      *val = LSBi8(*val);
   // ignore exception; we just use a return code
   if (xsink)
      xsink.clear();
   return rc;
}

int QoreSocket::recvu1(int timeout, unsigned char* val) {
   ExceptionSink xsink;
   int rc = qore_socket_exec_recv_integer(this, "recvu1", 1, val, timeout, &xsink);
   // ignore exception; we just use a return code
   if (xsink)
      xsink.clear();
   return rc;
}

int QoreSocket::recvu2(int timeout, unsigned short *val) {
   ExceptionSink xsink;
   int rc = qore_socket_exec_recv_integer(this, "recvu2", 2, val, timeout, &xsink);
   if (rc > 0)
      *val = ntohs(*val);
   // ignore exception; we just use a return code
   if (xsink)
      xsink.clear();
   return rc;
}

int QoreSocket::recvu4(int timeout, unsigned int* val) {
   ExceptionSink xsink;
   int rc = qore_socket_exec_recv_integer(this, "recvu4", 4, val, timeout, &xsink);
   if (rc > 0)
      *val = ntohl(*val);
   // ignore exception; we just use a return code
   if (xsink)
      xsink.clear();
   return rc;
}

int QoreSocket::recvu2LSB(int timeout, unsigned short *val) {
   ExceptionSink xsink;
   int rc = qore_socket_exec_recv_integer(this, "recvu2LSB", 2, val, timeout, &xsink);
   if (rc > 0)
      *val = LSBi2(*val);
   // ignore exception; we just use a return code
   if (xsink)
      xsink.clear();
   return rc;
}

int QoreSocket::recvu4LSB(int timeout, unsigned int* val) {
   ExceptionSink xsink;
   int rc = qore_socket_exec_recv_integer(this, "recvu4LSB", 4, val, timeout, &xsink);
   if (rc > 0)
      *val = LSBi4(*val);
   // ignore exception; we just use a return code
   if (xsink)
      xsink.clear();
   return rc;
}

int64 QoreSocket::recvi1(int timeout, char* val, ExceptionSink* xsink) {
   return qore_socket_exec_recv_integer(this, "recvi1", 1, val, timeout, xsink);
}

int64 QoreSocket::recvi2(int timeout, short *val, ExceptionSink* xsink) {
   int rc = qore_socket_exec_recv_integer(this, "recvi2", 2, val, timeout, xsink);
   if (rc > 0)
      *val = ntohs(*val);
   return rc;
}

int64 QoreSocket::recvi4(int timeout, int* val, ExceptionSink* xsink) {
   int rc = qore_socket_exec_recv_integer(this, "recvi4", 4, val, timeout, xsink);
   if (rc > 0)
      *val = ntohl(*val);
   return rc;
}

int64 QoreSocket::recvi8(int timeout, int64 *val, ExceptionSink* xsink) {
   int rc = qore_socket_exec_recv_integer(this, "recvi8", 8, val, timeout, xsink);
   if (rc > 0)
      *val = MSBi8(*val);
   return rc;
}

int64 QoreSocket::recvi2LSB(int timeout, short *val, ExceptionSink* xsink) {
   int rc = qore_socket_exec_recv_integer(this, "recvi2LSB", 2, val, timeout, xsink);
   if (rc > 0)
      *val = LSBi2(*val);
   return rc;
}

int64 QoreSocket::recvi4LSB(int timeout, int* val, ExceptionSink* xsink) {
   int rc = qore_socket_exec_recv_integer(this, "recvi4LSB", 4, val, timeout, xsink);
   if (rc > 0)
      *val = LSBi4(*val);
   return rc;
}

int64 QoreSocket::recvi8LSB(int timeout, int64 *val, ExceptionSink* xsink) {
   int rc = qore_socket_exec_recv_integer(this, "recvi8LSB", 8, val, timeout, xsink);
   if (rc > 0)
      *val = LSBi8(*val);
   return rc;
}

int64 QoreSocket::recvu1(int timeout, unsigned char* val, ExceptionSink* xsink) {
   return qore_socket_exec_recv_integer(this, "recvu1", 1, val, timeout, xsink);
}

int64 QoreSocket::recvu2(int timeout, unsigned short *val, ExceptionSink* xsink) {
   int rc = qore_socket_exec_recv_integer(this, "recvu2", 2, val, timeout, xsink);
   if (rc > 0)
      *val = ntohs(*val);
   return rc;
}

int64 QoreSocket::recvu4(int timeout, unsigned int* val, ExceptionSink* xsink) {
   int rc = qore_socket_exec_recv_integer(this, "recvu4", 4, val, timeout, xsink);
   if (rc > 0)
      *val = ntohl(*val);
   return rc;
}

int64 QoreSocket::recvu2LSB(int timeout, unsigned short *val, ExceptionSink* xsink) {
   int rc = qore_socket_exec_recv_integer(this, "recvu2LSB", 2, val, timeout, xsink);
   if (rc > 0)
      *val = LSBi2(*val);
   return rc;
}

int64 QoreSocket::recvu4LSB(int timeout, unsigned int* val, ExceptionSink* xsink) {
   int rc = qore_socket_exec_recv_integer(this, "recvu4LSB", 4, val, timeout, xsink);
   if (rc > 0)
      *val = LSBi4(*val);
   return rc;
}

int QoreSocket::send(int fd, qore_offset_t size) {
    if (!size) {
        return -1;
    }

    ExceptionSink xsink;
    SimpleRefHolder<FileInputStream> is(new FileInputStream(fd));
    int rc = qore_socket_exec_send_input_stream_poll(this, *is, size, -1, &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

int QoreSocket::send(int fd, qore_offset_t size, int timeout_ms, ExceptionSink* xsink) {
    assert(xsink);
    if (!size) {
        return 0;
    }

    SimpleRefHolder<FileInputStream> is(new FileInputStream(fd));
    return qore_socket_exec_send_input_stream_poll(this, *is, size, timeout_ms, xsink);
}

BinaryNode* QoreSocket::recvBinary(qore_offset_t bufsize, int timeout, int* rc) {
    assert(rc);
    ExceptionSink xsink;
    BinaryNode* b = qore_socket_exec_recv_binary(this, bufsize, timeout, &xsink);
    *rc = xsink ? -1 : b ? 0 : -1;
    // ignore exception; we just use a return code
    if (xsink)
        xsink.clear();
    return b;
}

BinaryNode* QoreSocket::recvBinary(int timeout, int* rc) {
    assert(rc);
    ExceptionSink xsink;
    BinaryNode* b = qore_socket_exec_recv_binary(this, 0, timeout, &xsink);
    *rc = xsink ? -1 : b ? 0 : -1;
    // ignore exception; we just use a return code
    if (xsink) {
        xsink.clear();
    }
    return b;
}

BinaryNode* QoreSocket::recvBinary(qore_offset_t bufsize, int timeout, ExceptionSink* xsink) {
    assert(xsink);
    BinaryNodeHolder b(qore_socket_exec_recv_binary(this, bufsize, timeout, xsink));
    return *xsink ? 0 : b.release();
}

BinaryNode* QoreSocket::recvBinary(int timeout, ExceptionSink* xsink) {
    assert(xsink);
    BinaryNodeHolder b(qore_socket_exec_recv_binary(this, 0, timeout, xsink));
    return *xsink ? 0 : b.release();
}

QoreStringNode* QoreSocket::recv(qore_offset_t bufsize, int timeout, int* rc) {
    assert(rc);
    ExceptionSink xsink;
    QoreStringNode* str = qore_socket_exec_recv_string(this, bufsize, timeout, &xsink);
    *rc = xsink ? -1 : str ? 0 : -1;
    // ignore exceptions; we use only a return code
    if (xsink) {
        xsink.clear();
    }
    return str;
}

QoreStringNode* QoreSocket::recv(int timeout, int* rc) {
    assert(rc);
    ExceptionSink xsink;
    QoreStringNode* str = qore_socket_exec_recv_string(this, 0, timeout, &xsink);
    *rc = xsink ? -1 : str ? 0 : -1;
    // ignore exceptions; we use only a return code
    if (xsink) {
        xsink.clear();
    }
    return str;
}

QoreStringNode* QoreSocket::recv(qore_offset_t bufsize, int timeout, ExceptionSink* xsink) {
    assert(xsink);
    QoreStringNodeHolder str(qore_socket_exec_recv_string(this, bufsize, timeout, xsink));
    return *xsink ? 0 : str.release();
}

QoreStringNode* QoreSocket::recv(int timeout, ExceptionSink* xsink) {
    assert(xsink);
    QoreStringNodeHolder str(qore_socket_exec_recv_string(this, 0, timeout, xsink));
    return *xsink ? 0 : str.release();
}

// receive data and write to file descriptor
int QoreSocket::recv(int fd, qore_offset_t size, int timeout_ms, ExceptionSink* xsink) {
    assert(xsink);
    if (!size) {
        return 0;
    }

    SimpleRefHolder<FileOutputStream> os(new FileOutputStream(fd));
    return qore_socket_exec_recv_output_stream_poll(this, *os, size, timeout_ms, xsink);
}

// receive data and write to file descriptor
int QoreSocket::recv(int fd, qore_offset_t size, int timeout) {
    if (!size) {
        return -1;
    }

    ExceptionSink xsink;
    SimpleRefHolder<FileOutputStream> os(new FileOutputStream(fd));
    int rc = qore_socket_exec_recv_output_stream_poll(this, *os, size, timeout, &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

// returns 0 for success
int QoreSocket::sendHTTPMessage(const char* method, const char* path, const char* http_version,
        const QoreHashNode* headers, const void *data, size_t size, int source) {
    ExceptionSink xsink;
    int rc = qore_socket_exec_send_http_message(this, nullptr, method, path, http_version, headers, data, size,
        nullptr, source, -1, &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

// returns 0 for success
int QoreSocket::sendHTTPMessage(QoreHashNode* info, const char* method, const char* path, const char* http_version,
        const QoreHashNode* headers, const void *data, size_t size, int source) {
    ExceptionSink xsink;
    int rc = qore_socket_exec_send_http_message(this, info, method, path, http_version, headers, data, size,
        nullptr, source, -1, &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

int QoreSocket::sendHTTPMessage(ExceptionSink* xsink, QoreHashNode* info, const char* method, const char* path,
        const char* http_version, const QoreHashNode* headers, const void *data, size_t size, int source) {
    return qore_socket_exec_send_http_message(this, info, method, path, http_version, headers, data, size,
        nullptr, source, -1, xsink);
}

int QoreSocket::sendHTTPMessage(ExceptionSink* xsink, QoreHashNode* info, const char* method, const char* path,
        const char* http_version, const QoreHashNode* headers, const void *data, size_t size, int source,
        int timeout_ms) {
    return qore_socket_exec_send_http_message(this, info, method, path, http_version, headers, data, size,
        nullptr, source, timeout_ms, xsink);
}

int QoreSocket::sendHTTPMessageWithCallback(ExceptionSink* xsink, QoreHashNode *info, const char* method,
        const char *path, const char *http_version, const QoreHashNode *headers,
        const ResolvedCallReferenceNode& send_callback, int source, int timeout_ms) {
    return qore_socket_exec_send_http_message_callback(this, info, method, path, http_version, headers,
        &send_callback, source, timeout_ms, nullptr, xsink);
}

// returns 0 for success
int QoreSocket::sendHTTPResponse(int code, const char* desc, const char* http_version, const QoreHashNode* headers,
    const void *data, size_t size, int source) {
    ExceptionSink xsink;
    int rc = qore_socket_exec_send_http_response(this, nullptr, code, desc, http_version, headers, data, size,
        nullptr, source, -1, &xsink);
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

int QoreSocket::sendHTTPResponse(ExceptionSink* xsink, int code, const char* desc, const char* http_version,
    const QoreHashNode* headers, const void *data, size_t size, int source) {
    return qore_socket_exec_send_http_response(this, nullptr, code, desc, http_version, headers, data, size,
        nullptr, source, -1, xsink);
}

int QoreSocket::sendHTTPResponse(ExceptionSink* xsink, int code, const char* desc, const char* http_version,
    const QoreHashNode* headers, const void *data, size_t size, int source, int timeout_ms) {
    return qore_socket_exec_send_http_response(this, nullptr, code, desc, http_version, headers, data, size,
        nullptr, source, timeout_ms, xsink);
}

int QoreSocket::sendHTTPResponse(ExceptionSink* xsink, QoreHashNode* info, int code, const char* desc,
    const char* http_version, const QoreHashNode* headers, const void *data, size_t size, int source,
    int timeout_ms) {
    return qore_socket_exec_send_http_response(this, info, code, desc, http_version, headers, data, size,
        nullptr, source, timeout_ms, xsink);
}

AbstractQoreNode* QoreSocket::readHTTPHeader(int timeout, int* rc, int source) {
    assert(rc);
    ExceptionSink xsink;
    qore_offset_t nrc;
    AbstractQoreNode* n = qore_socket_exec_read_http_header(this, nullptr, timeout, &nrc, source, &xsink);
    *rc = (int)nrc;
    // ignore exception; we just use a return code
    if (xsink)
        xsink.clear();
    return n;
}

// rc is:
//    0 for remote end shutdown
//   -1 for socket error
//   -2 for socket not open
//   -3 for timeout
AbstractQoreNode* QoreSocket::readHTTPHeader(QoreHashNode* info, int timeout, int* rc, int source) {
    assert(rc);
    ExceptionSink xsink;
    qore_offset_t nrc;
    AbstractQoreNode* n = qore_socket_exec_read_http_header(this, info, timeout, &nrc, source, &xsink);
    *rc = (int)nrc;
    // ignore exception; we just use a return code
    if (xsink)
        xsink.clear();
    return n;
}

QoreHashNode* QoreSocket::readHTTPHeader(ExceptionSink* xsink, QoreHashNode* info, int timeout, int source) {
    assert(xsink);
    return qore_socket_exec_read_http_header(this, info, timeout, nullptr, source, xsink);
}

QoreStringNode* QoreSocket::readHTTPHeaderString(ExceptionSink* xsink, int timeout, int source) {
    assert(xsink);
    return qore_socket_exec_read_http_header_string(this, timeout, source, xsink);
}

// receive a binary message in HTTP chunked format
QoreHashNode* QoreSocket::readHTTPChunkedBodyBinary(int timeout, ExceptionSink* xsink, int source) {
    return qore_socket_exec_read_http_chunked_body(this, timeout, true, false, "readHTTPChunkedBodyBinary",
        source, xsink);
}

// receive a message in HTTP chunked format
QoreHashNode* QoreSocket::readHTTPChunkedBody(int timeout, ExceptionSink* xsink, int source) {
    return qore_socket_exec_read_http_chunked_body(this, timeout, false, false, "readHTTPChunkedBody",
        source, xsink);
}

QoreHashNode* QoreSocket::readHttpChunk(int timeout, ExceptionSink* xsink) {
    return qore_socket_exec_read_http_chunked_body(this, timeout, true, true, "readHTTPChunk",
        QORE_SOURCE_SOCKET, xsink);
}

QoreHashNode* QoreSocket::parseServerSentEvent(ExceptionSink* xsink, const QoreString& buf) {
    return parseSseEvent(xsink, buf);
}

QoreHashNode* QoreSocket::readServerSentEvent(ExceptionSink* xsink, const QoreStringNode* content_encoding,
        int timeout_ms) {
    if (content_encoding && (*content_encoding != "identity")) {
        SimpleRefHolder<Transform> t(CompressionTransforms::getDecompressor(content_encoding, xsink));
        if (*xsink) {
            return nullptr;
        }
        return qore_socket_exec_read_server_sent_event_encoded(this, content_encoding, timeout_ms, xsink);
    }
    return qore_socket_exec_read_server_sent_event(this, timeout_ms, xsink);
}

bool QoreSocket::isDataAvailable(int timeout) const {
    ExceptionSink xsink;
    int rc = qore_socket_exec_is_data_available(const_cast<QoreSocket*>(this), timeout, &xsink);
    // ignore exception; we just use a return code
    if (xsink)
        xsink.clear();
    return rc;
}

bool QoreSocket::isWriteFinished(int timeout) const {
    ExceptionSink xsink;
    int rc = qore_socket_exec_wait_readiness(const_cast<QoreSocket*>(this), timeout, SOCK_POLLOUT,
        "isWriteFinished", "waiting-write", "write-ready", &xsink);
    // ignore exception; we just use a return code
    if (xsink)
        xsink.clear();
    return rc;
}

bool QoreSocket::isDataAvailable(ExceptionSink* xsink, int timeout) const {
    return qore_socket_exec_is_data_available(const_cast<QoreSocket*>(this), timeout, xsink);
}

bool QoreSocket::isWriteFinished(ExceptionSink* xsink, int timeout) const {
    return qore_socket_exec_wait_readiness(const_cast<QoreSocket*>(this), timeout, SOCK_POLLOUT,
        "isWriteFinished", "waiting-write", "write-ready", xsink) > 0;
}

int QoreSocket::asyncIoWait(int timeout_ms, bool read, bool write) const {
    assert(read || write);
    if (!read && !write) {
        return 0;
    }

    ExceptionSink xsink;
    int events = (read ? SOCK_POLLIN : 0) | (write ? SOCK_POLLOUT : 0);
    const char* waiting_state = read && write ? "waiting-io" : read ? "waiting-read" : "waiting-write";
    const char* ready_state = read && write ? "io-ready" : read ? "read-ready" : "write-ready";
    int rc = qore_socket_exec_wait_readiness(const_cast<QoreSocket*>(this), timeout_ms, events, "asyncIoWait",
        waiting_state, ready_state, &xsink);
    // ignore exception; we just use a return code
    if (xsink)
        xsink.clear();
    return rc;
}

int QoreSocket::upgradeClientToSSL(ExceptionSink* xsink, int timeout_ms, QoreSSLCertificate* cert,
        QoreSSLPrivateKey* pkey) {
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_ALL);
    if (!io_guard) {
        return -1;
    }

    ValueHolder rv(qore_socket_exec_poll(this,
        new QoreSocketControllerSslUpgradePollOperation(this, false, cert, pkey),
        timeout_ms, "upgradeClientToSSL", "ssl-upgraded", xsink), xsink);
    return *xsink ? -1 : 0;
}

int QoreSocket::upgradeServerToSSL(ExceptionSink* xsink, int timeout_ms, QoreSSLCertificate* cert,
        QoreSSLPrivateKey* pkey) {
    QoreSocketRawAsyncIoGuard io_guard(*priv, xsink, NB_ALL);
    if (!io_guard) {
        return -1;
    }

    ValueHolder rv(qore_socket_exec_poll(this,
        new QoreSocketControllerSslUpgradePollOperation(this, true, cert, pkey),
        timeout_ms, "upgradeServerToSSL", "ssl-upgraded", xsink), xsink);
    return *xsink ? -1 : 0;
}

static bool qore_socket_parse_bind_name(const char* bind_name, std::string& host, std::string& service,
        int& family) {
    const char* p = strrchr(bind_name, ':');
    if (!p) {
        return false;
    }

    host.assign(bind_name, p - bind_name);
    service = p + 1;

    // If the address is bracketed like [<addr>]:<port>, bind as IPv6.
    if (host.size() > 2 && host.front() == '[' && host.back() == ']') {
        host.erase(host.size() - 1);
        host.erase(0, 1);
        family = AF_INET6;
    } else {
        // Otherwise preserve the legacy parser: a colon in the host portion means IPv6, else IPv4.
        family = host.find(':') == std::string::npos ? AF_INET : AF_INET6;
    }
    return true;
}

static int qore_socket_bind_unix_direct(QoreSocket* s, const char* name, int socktype, int protocol,
        ExceptionSink* xsink) {
    qore_socket_private* priv = qore_socket_private::get(*s);
    qore_socket_close_private_from_controller(priv);
    return priv->bindUNIX(xsink, name, socktype, protocol);
}

static int qore_socket_bind_inet_resolved_direct(QoreSocket* s, const char* name, const char* service,
        bool reuseaddr, int protocol, std::vector<SocketResolvedAddrInfo>& addrs, ExceptionSink* xsink) {
    qore_socket_private* priv = qore_socket_private::get(*s);
    qore_socket_close_private_from_controller(priv);

    if (addrs.empty()) {
        xsink->raiseException("QOREADDRINFO-GETINFO-ERROR",
            "ares_getaddrinfo(node: '%s', service: '%s') returned no addresses", name ? name : "",
            service ? service : "");
        return -1;
    }

    if (priv->hasEventQueue()) {
        for (auto& ai : addrs) {
            priv->do_resolved_event(reinterpret_cast<const struct sockaddr*>(&ai.addr));
        }
    }

    const SocketResolvedAddrInfo& first = addrs.front();
    if (priv->openINET(first.family, first.socktype, protocol)) {
        qore_socket_error(xsink, "SOCKET-BINDINET-ERROR", "error opening socket for bind", 0, name, service);
        return -1;
    }

    int prt = q_get_port_from_addr(reinterpret_cast<const struct sockaddr*>(&first.addr));
    int en = 0;
    for (auto& ai : addrs) {
        if (!priv->bindIntern(reinterpret_cast<struct sockaddr*>(&ai.addr), ai.addrlen, prt, reuseaddr)) {
            return 0;
        }
        en = sock_get_raw_error();
    }

    qore_socket_error_intern(en, xsink, "SOCKET-BIND-ERROR", "error binding on socket", 0, name, service);
    return -1;
}

QoreSocketControllerSetupPollOperation::~QoreSocketControllerSetupPollOperation() = default;

QoreHashNode* QoreSocketControllerSetupPollOperation::getResolverPollInfo(ExceptionSink* xsink) const {
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

QoreHashNode* QoreSocketControllerSetupPollOperation::continueBindInet(ExceptionSink* xsink) {
    if (!bind_inet_resolved) {
        if (!resolver) {
            qore_socket_private::get(*sock)->do_resolve_event(has_name ? name.c_str() : nullptr,
                has_service ? service.c_str() : nullptr);
            resolver = std::make_unique<QoreCaresAddrInfoResolver>(
                has_name ? name.c_str() : nullptr,
                has_service ? service.c_str() : nullptr,
                q_get_af(family), q_get_sock_type(socktype), protocol, AI_PASSIVE);
        }

        int rrc = resolver->continuePoll(xsink);
        if (*xsink || rrc < 0) {
            done = true;
            return nullptr;
        }
        if (rrc) {
            return getResolverPollInfo(xsink);
        }

        bind_inet_addrs = resolver->getAddresses();
        resolver.reset();
        bind_inet_resolved = true;
    }

    rc = qore_socket_bind_inet_resolved_direct(sock, has_name ? name.c_str() : nullptr,
        has_service ? service.c_str() : nullptr, reuseaddr, protocol, bind_inet_addrs, xsink);
    done = true;
    return nullptr;
}

SocketSetupPollOperation::~SocketSetupPollOperation() = default;

QoreHashNode* SocketSetupPollOperation::getResolverPollInfo(ExceptionSink* xsink) const {
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

QoreHashNode* SocketSetupPollOperation::continueBindInet(ExceptionSink* xsink) {
    if (!bind_inet_resolved) {
        if (!resolver) {
            qore_socket_private::get(*sock->priv->socket)->do_resolve_event(has_name ? name.c_str() : nullptr,
                has_service ? service.c_str() : nullptr);
            resolver = std::make_unique<QoreCaresAddrInfoResolver>(
                has_name ? name.c_str() : nullptr,
                has_service ? service.c_str() : nullptr,
                q_get_af(family), q_get_sock_type(socktype), protocol, AI_PASSIVE);
        }

        int rrc = resolver->continuePoll(xsink);
        if (*xsink || rrc < 0) {
            clearNonBlockLocked();
            done = true;
            return nullptr;
        }
        if (rrc) {
            return getResolverPollInfo(xsink);
        }

        bind_inet_addrs = resolver->getAddresses();
        resolver.reset();
        bind_inet_resolved = true;
    }

    rc = qore_socket_bind_inet_resolved_direct(sock->priv->socket, has_name ? name.c_str() : nullptr,
        has_service ? service.c_str() : nullptr, reuseaddr, protocol, bind_inet_addrs, xsink);
    clearNonBlockLocked();
    done = true;
    return nullptr;
}

static int qore_socket_bind_check_sandbox(const struct sockaddr* addr, int size, int socktype,
        ExceptionSink* xsink) {
    QoreSandboxManagerHelper smh;
    if (smh) {
        int proto = socktype == SOCK_STREAM ? QSEC_NET_TCP : socktype == SOCK_DGRAM ? QSEC_NET_UDP : QSEC_NET_ALL;
        if (!smh->checkNetworkAccess(addr, size, proto, xsink)) {
            return -1;
        }
    }
    return 0;
}

static int qore_socket_bind_sockaddr_direct(QoreSocket* s, const struct sockaddr* addr, int size,
        ExceptionSink* xsink) {
    qore_socket_private* priv = qore_socket_private::get(*s);

    if (qore_socket_bind_check_sandbox(addr, size, SOCK_STREAM, xsink)) {
        return -1;
    }

    // close if it's already been opened as an INET socket or with different parameters
    if (priv->sock != QORE_INVALID_SOCKET && (priv->sfamily != AF_INET || priv->stype != SOCK_STREAM
        || priv->sprot != 0))
        qore_socket_close_private_from_controller(priv);

    // try to open socket if necessary
    if (priv->sock == QORE_INVALID_SOCKET && priv->openINET())
        return -1;

    if ((::bind(priv->sock, addr, size)) == QORE_SOCKET_ERROR) {
#ifdef _Q_WINDOWS
        // set errno from windows error
        sock_get_error();
#endif
        int bind_errno = errno;
        qore_socket_close_private_from_controller(priv);
        errno = bind_errno;
        return -1;
    }

    // set port number to unknown
    priv->port = -1;
    //printd(5, "QoreSocket::bind(interface, port) returning 0 (success)\n");
    return 0;
}

static int qore_socket_bind_family_sockaddr_direct(QoreSocket* s, int family, const struct sockaddr* addr,
        int size, int sock_type, int protocol, ExceptionSink* xsink) {
    qore_socket_private* priv = qore_socket_private::get(*s);

    family = q_get_af(family);
    sock_type = q_get_sock_type(sock_type);

    if (qore_socket_bind_check_sandbox(addr, size, sock_type, xsink)) {
        return -1;
    }

    // close if it's already been opened as an INET socket or with different parameters
    if (priv->sock != QORE_INVALID_SOCKET && (priv->sfamily != family || priv->stype != sock_type
        || priv->sprot != protocol))
        qore_socket_close_private_from_controller(priv);

    // try to open socket if necessary
    if (priv->sock == QORE_INVALID_SOCKET && priv->openINET(family, sock_type, protocol))
        return -1;

    if ((::bind(priv->sock, addr, size)) == -1) {
#ifdef _Q_WINDOWS
        // set errno from windows error
        sock_get_error();
#endif
        int bind_errno = errno;
        qore_socket_close_private_from_controller(priv);
        errno = bind_errno;
        return -1;
    }

    // set port number
    int prt = q_get_port_from_addr(addr);
    priv->port = prt ? prt : -1;
    //printd(5, "QoreSocket::bind(interface, port) returning 0 (success)\n");
    return 0;
}

static int qore_socket_listen_direct(QoreSocket* s, int backlog) {
    return qore_socket_private::get(*s)->listen(backlog);
}

static int qore_socket_shutdown_direct(QoreSocket* s) {
    return qore_socket_private::get(*s)->shutdown_direct();
}

static int qore_socket_set_no_delay_direct(QoreSocket* s, int nodelay) {
    qore_socket_private* priv = qore_socket_private::get(*s);
    return setsockopt(priv->sock, IPPROTO_TCP, TCP_NODELAY, (SETSOCKOPT_ARG_4)&nodelay, sizeof(int));
}

static int qore_socket_get_no_delay_direct(QoreSocket* s) {
    qore_socket_private* priv = qore_socket_private::get(*s);
    int rc;
    socklen_t optlen = sizeof(int);
    int sorc = getsockopt(priv->sock, IPPROTO_TCP, TCP_NODELAY, (GETSOCKOPT_ARG_4)&rc, &optlen);
    //printd(5, "Socket::getNoDelay() sorc: %d rc: %d optlen: %d\n", sorc, rc, optlen);
    if (sorc) {
        return sorc;
    }
    return rc;
}

static int qore_socket_set_user_timeout_direct(QoreSocket* s, int ms) {
    qore_socket_private* priv = qore_socket_private::get(*s);
    if (ms < 0) {
        ms = 0;
    }
    priv->tcp_user_timeout_ms = ms;
#ifdef TCP_USER_TIMEOUT
    if (priv->sock != QORE_INVALID_SOCKET
            && (priv->sfamily == AF_INET || priv->sfamily == AF_INET6)
            && priv->stype == SOCK_STREAM) {
        unsigned int v = (unsigned int)ms;
        return setsockopt(priv->sock, IPPROTO_TCP, TCP_USER_TIMEOUT,
            (SETSOCKOPT_ARG_4)&v, sizeof(v));
    }
#endif
    return 0;
}

static int qore_socket_get_user_timeout_direct(QoreSocket* s) {
    qore_socket_private* priv = qore_socket_private::get(*s);
#ifdef TCP_USER_TIMEOUT
    if (priv->sock != QORE_INVALID_SOCKET
            && (priv->sfamily == AF_INET || priv->sfamily == AF_INET6)
            && priv->stype == SOCK_STREAM) {
        unsigned int v = 0;
        socklen_t optlen = sizeof(v);
        int sorc = getsockopt(priv->sock, IPPROTO_TCP, TCP_USER_TIMEOUT,
            (GETSOCKOPT_ARG_4)&v, &optlen);
        if (sorc == 0) {
            return (int)v;
        }
        // getsockopt failed (e.g., kernel rejected) — fall back to cached
    }
#endif
    return priv->tcp_user_timeout_ms;
}

static int qore_socket_set_socket_timeout_direct(QoreSocket* s, int optname, int ms) {
    qore_socket_private* priv = qore_socket_private::get(*s);
    struct timeval tv;
    tv.tv_sec  = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;

    return setsockopt(priv->sock, SOL_SOCKET, optname, (SETSOCKOPT_ARG_4)&tv, sizeof(struct timeval));
}

static int qore_socket_get_socket_timeout_direct(QoreSocket* s, int optname) {
    qore_socket_private* priv = qore_socket_private::get(*s);
    return optname == SO_SNDTIMEO ? priv->getSendTimeout() : priv->getRecvTimeout();
}

static int qore_socket_get_port_direct(QoreSocket* s) {
    return qore_socket_private::get(*s)->getPort();
}

/* currently hardcoded to SOCK_STREAM (tcp-only)
   if there is no port specifier, opens UNIX domain socket (if necessary)
   and binds to a local UNIX socket file
   for UNIX domain sockets: AF_UNIX
   - bind("filename");
   for ipv4 (unless an ipv6 address is detected in the host part): AF_INET
   - bind("interface:port");
   for ipv6 sockets: AF_INET6
   - bind("[interface]:port");
*/
int QoreSocket::bind(const char* name, bool reuseaddr) {
    return qore_socket_exec_setup_no_exception(this,
        new QoreSocketControllerSetupPollOperation(this, name, reuseaddr), "bind");
}

int QoreSocket::bindUNIX(const char* name, int socktype, int protocol, ExceptionSink* xsink) {
    return qore_socket_exec_setup(this,
        new QoreSocketControllerSetupPollOperation(this, name, socktype, protocol), "bindUNIX", xsink);
}

int QoreSocket::bindINET(const char* name, const char* service, bool reuseaddr, int family, int socktype,
        int protocol, ExceptionSink* xsink) {
    return qore_socket_exec_setup(this,
        new QoreSocketControllerSetupPollOperation(this, name, service, reuseaddr, family, socktype, protocol),
        "bindINET", xsink);
}

// currently hardcoded to SOCK_STREAM (tcp-only)
// opens INET socket and binds to a tcp port on all interfaces
// closes socket if already open, because the socket will be
// bound to all interfaces
// * bind(port);
int QoreSocket::bind(int prt, bool reuseaddr) {
    return qore_socket_exec_setup_no_exception(this,
        new QoreSocketControllerSetupPollOperation(this, prt, reuseaddr), "bind");
}

// to bind to an INET tcp port on a specific interface
int QoreSocket::bind(const char* iface, int prt, bool reuseaddr) {
    return qore_socket_exec_setup_no_exception(this,
        new QoreSocketControllerSetupPollOperation(this, iface, prt, reuseaddr), "bind");
}

// to bind an INET socket to a particular address
int QoreSocket::bind(const struct sockaddr *addr, int size) {
    ExceptionSink xsink;
    int rc = qore_socket_exec_setup(this,
        new QoreSocketControllerSetupPollOperation(this, addr, size, &xsink), "bind", &xsink);
    // ignore exception; we just use a return code
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

int QoreSocket::bind(int family, const struct sockaddr *addr, int size, int sock_type, int protocol) {
    ExceptionSink xsink;
    int rc = qore_socket_exec_setup(this,
        new QoreSocketControllerSetupPollOperation(this, family, addr, size, sock_type, protocol, &xsink),
        "bind", &xsink);
    // ignore exception; we just use a return code
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

// find out what port we're connected to
int QoreSocket::getPort() {
    return qore_socket_exec_setup_no_exception(this,
        new QoreSocketControllerSetupPollOperation(this,
            QoreSocketControllerSetupPollOperation::ConfigAction::GetPort),
        "getPort");
}

// QoreSocket::accept()
// returns a new socket
QoreSocket* QoreSocket::accept(SocketSource* source, ExceptionSink* xsink) {
    return qore_socket_exec_accept_socket(this, source, -1, xsink);
}

// QoreSocket::acceptSSL()
// accepts a new connection, negotiates an SSL connection, and returns the new socket
QoreSocket* QoreSocket::acceptSSL(ExceptionSink* xsink, SocketSource* source, QoreSSLCertificate* cert,
        QoreSSLPrivateKey* pkey) {
    return qore_socket_exec_accept_ssl_socket(this, source, -1, cert, pkey, xsink);
}

// accept a connection and replace the socket with the new connection
int QoreSocket::acceptAndReplace(SocketSource* source) {
    QORE_TRACE("QoreSocket::acceptAndReplace()");
    ExceptionSink xsink;
    int rc = qore_socket_exec_accept_replace(this, source, -1, &xsink);
    // ignore exception; we just use a return code
    if (xsink) {
        xsink.clear();
        return -1;
    }
    return rc;
}

QoreSocket* QoreSocket::accept(int timeout_ms, ExceptionSink* xsink) {
    return qore_socket_exec_accept_socket(this, nullptr, timeout_ms, xsink);
}

QoreSocket* QoreSocket::acceptSSL(ExceptionSink* xsink, int timeout_ms, QoreSSLCertificate* cert,
        QoreSSLPrivateKey* pkey) {
    return qore_socket_exec_accept_ssl_socket(this, nullptr, timeout_ms, cert, pkey, xsink);
}

int QoreSocket::acceptAndReplace(int timeout_ms, ExceptionSink* xsink) {
    return qore_socket_exec_accept_replace(this, nullptr, timeout_ms, xsink);
}

int QoreSocket::listen(int backlog) {
    return qore_socket_exec_setup_no_exception(this,
        new QoreSocketControllerSetupPollOperation(this, backlog), "listen");
}

int QoreSocket::listen() {
    return listen(20);
}

int QoreSocket::send(const char* buf, size_t size) {
    return qore_socket_exec_send_bytes_no_exception(this, buf, size, -1);
}

int QoreSocket::send(const char* buf, size_t size, ExceptionSink* xsink) {
    return qore_socket_exec_send_bytes(this, buf, size, -1, xsink);
}

int QoreSocket::send(const char* buf, size_t size, int timeout_ms, ExceptionSink* xsink) {
    return qore_socket_exec_send_bytes(this, buf, size, timeout_ms, xsink);
}

// converts to socket encoding if necessary
int QoreSocket::send(const QoreString* msg, ExceptionSink* xsink) {
    TempEncodingHelper tstr(msg, priv->enc, xsink);
    if (!tstr)
        return -1;

    return qore_socket_exec_send_bytes(this, tstr->c_str(), tstr->strlen(), -1, xsink);
}

// converts to socket encoding if necessary
int QoreSocket::send(const QoreString* msg, int timeout_ms, ExceptionSink* xsink) {
    TempEncodingHelper tstr(msg, priv->enc, xsink);
    if (!tstr)
        return -1;

    return qore_socket_exec_send_bytes(this, tstr->c_str(), tstr->strlen(), timeout_ms, xsink);
}

// converts to socket encoding if necessary
int QoreSocket::send(const QoreStringNode& msg, int timeout_ms, ExceptionSink* xsink) {
    QoreStringNodeValueHelper tstr(&msg, priv->enc, xsink);
    if (*xsink)
        return -1;

    return qore_socket_exec_send_bytes(this, tstr->c_str(), tstr->strlen(), timeout_ms, xsink);
}

int QoreSocket::send(const BinaryNode* b) {
    return qore_socket_exec_send_bytes_no_exception(this, b->getPtr(), b->size(), -1);
}

int QoreSocket::send(const BinaryNode* b, ExceptionSink* xsink) {
    return qore_socket_exec_send_bytes(this, b->getPtr(), b->size(), -1, xsink);
}

int QoreSocket::send(const BinaryNode* b, int timeout_ms, ExceptionSink* xsink) {
    return qore_socket_exec_send_bytes(this, b->getPtr(), b->size(), timeout_ms, xsink);
}

int QoreSocket::setSendTimeout(int ms) {
    return qore_socket_exec_setup_no_exception(this,
        new QoreSocketControllerSetupPollOperation(this,
            QoreSocketControllerSetupPollOperation::ConfigAction::SetSendTimeout, ms),
        "setSendTimeout");
}

int QoreSocket::setRecvTimeout(int ms) {
    return qore_socket_exec_setup_no_exception(this,
        new QoreSocketControllerSetupPollOperation(this,
            QoreSocketControllerSetupPollOperation::ConfigAction::SetRecvTimeout, ms),
        "setRecvTimeout");
}

int QoreSocket::getSendTimeout() const {
    return qore_socket_exec_setup_no_exception(const_cast<QoreSocket*>(this),
        new QoreSocketControllerSetupPollOperation(const_cast<QoreSocket*>(this),
            QoreSocketControllerSetupPollOperation::ConfigAction::GetSendTimeout),
        "getSendTimeout");
}

int QoreSocket::getRecvTimeout() const {
    return qore_socket_exec_setup_no_exception(const_cast<QoreSocket*>(this),
        new QoreSocketControllerSetupPollOperation(const_cast<QoreSocket*>(this),
            QoreSocketControllerSetupPollOperation::ConfigAction::GetRecvTimeout),
        "getRecvTimeout");
}

void QoreSocket::setMaxChunkedBodySize(int64 size) {
    qore_socket_exec_setup_no_exception(this,
        new QoreSocketControllerSetupPollOperation(this,
            QoreSocketControllerSetupPollOperation::ConfigAction::SetMaxChunkedBodySize, size),
        "setMaxChunkedBodySize");
}

int64 QoreSocket::getMaxChunkedBodySize() const {
    return priv->max_chunked_body_size.load(std::memory_order_relaxed);
}

void QoreSocket::setHttp2MaxRequestBodySize(int64 size) {
    qore_socket_exec_setup_no_exception(this,
        new QoreSocketControllerSetupPollOperation(this,
            QoreSocketControllerSetupPollOperation::ConfigAction::SetHttp2MaxRequestBodySize, size),
        "setHttp2MaxRequestBodySize");
}

int64 QoreSocket::getHttp2MaxRequestBodySize() const {
    return priv->max_http2_body_size.load(std::memory_order_relaxed);
}

void QoreSocket::setEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
    priv->setEventQueue(xsink, q, arg, with_data);
}

Queue* QoreSocket::getQueue() {
    return priv->getEventQueue();
}

void QoreSocket::cleanup(ExceptionSink* xsink) {
    qore_socket_exec_close(this);
    if (priv->outer_lock) {
        AutoLocker al(*priv->outer_lock);
        priv->cleanupQueues(xsink);
    } else {
        priv->cleanupQueues(xsink);
    }
}

int64 QoreSocket::getObjectIDForEvents() const {
    return priv->getObjectIDForEvents();
}

QoreHashNode* QoreSocket::getPeerInfo(ExceptionSink* xsink) const {
    return getPeerInfo(xsink, true);
}

QoreHashNode* QoreSocket::getSocketInfo(ExceptionSink* xsink) const {
    return getSocketInfo(xsink, true);
}

QoreHashNode* QoreSocket::getPeerInfo(ExceptionSink* xsink, bool host_lookup) const {
    return qore_socket_exec_address_info(const_cast<QoreSocket*>(this),
        QoreSocketControllerAddressInfoPollOperation::Action::Peer, host_lookup, "getPeerInfo",
        "SOCKET-GETPEERINFO-ERROR", xsink);
}

QoreHashNode* QoreSocket::getSocketInfo(ExceptionSink* xsink, bool host_lookup) const {
    return qore_socket_exec_address_info(const_cast<QoreSocket*>(this),
        QoreSocketControllerAddressInfoPollOperation::Action::Socket, host_lookup, "getSocketInfo",
        "SOCKET-GETSOCKETINFO-ERROR", xsink);
}

void QoreSocket::setAccept(QoreObject *o) {
    ExceptionSink xsink;
    ReferenceHolder<QoreHashNode> info(qore_socket_exec_address_info(this,
        QoreSocketControllerAddressInfoPollOperation::Action::Peer, true, "setAccept",
        "SOCKET-GETPEERINFO-ERROR", &xsink), &xsink);
    if (!xsink && info) {
        qore_socket_private::setAccept(o, **info);
    }
    if (xsink) {
        xsink.clear();
    }
}

void QoreSocket::clearWarningQueue(ExceptionSink* xsink) {
    priv->clearWarningQueue(xsink);
}

void QoreSocket::setWarningQueue(ExceptionSink* xsink, int64 warning_ms, int64 warning_bs, Queue* wq, QoreValue arg, int64 min_ms) {
    priv->setWarningQueue(xsink, warning_ms, warning_bs, wq, arg, min_ms);
}

QoreHashNode* QoreSocket::getUsageInfo() const {
    return priv->getUsageInfo();
}

void QoreSocket::clearStats() {
    priv->clearStats();
}

bool QoreSocket::pendingHttpChunkedBody() const {
    return priv->pendingHttpChunkedBody();
}

void QoreSocket::setSslVerifyMode(int mode) {
    qore_socket_exec_setup_no_exception(this,
        new QoreSocketControllerSetupPollOperation(this,
            QoreSocketControllerSetupPollOperation::ConfigAction::SetSslVerifyMode, mode),
        "setSslVerifyMode");
}

int QoreSocket::getSslVerifyMode() const {
    return qore_socket_exec_setup_no_exception(const_cast<QoreSocket*>(this),
        new QoreSocketControllerSetupPollOperation(const_cast<QoreSocket*>(this),
            QoreSocketControllerSetupPollOperation::ConfigAction::GetSslVerifyMode),
        "getSslVerifyMode");
}

void QoreSocket::acceptAllCertificates(bool accept_all) {
    qore_socket_exec_setup_no_exception(this,
        new QoreSocketControllerSetupPollOperation(this,
            QoreSocketControllerSetupPollOperation::ConfigAction::SetAcceptAllCertificates, accept_all),
        "acceptAllCertificates");
}

bool QoreSocket::getAcceptAllCertificates() const {
    return qore_socket_exec_setup_no_exception(const_cast<QoreSocket*>(this),
        new QoreSocketControllerSetupPollOperation(const_cast<QoreSocket*>(this),
            QoreSocketControllerSetupPollOperation::ConfigAction::GetAcceptAllCertificates),
        "getAcceptAllCertificates") > 0;
}

bool QoreSocket::captureRemoteCertificates(bool set) {
    return qore_socket_exec_setup_no_exception(this,
        new QoreSocketControllerSetupPollOperation(this,
            QoreSocketControllerSetupPollOperation::ConfigAction::CaptureRemoteCertificates, set),
        "captureRemoteCertificates") > 0;
}

QoreObject* QoreSocket::getRemoteCertificate() const {
    if (qore_on_async_io_thread()) {
        if (priv->remote_cert) {
            priv->remote_cert->ref();
            return priv->remote_cert;
        }
        return nullptr;
    }

    ExceptionSink xsink;
    QoreObject* rv = qore_socket_exec_tls_state_object(const_cast<QoreSocket*>(this),
        QoreSocketControllerTlsStatePollOperation::Action::GetRemoteCertificate, "getRemoteCertificate", &xsink);
    if (xsink) {
        xsink.clear();
        return nullptr;
    }
    return rv;
}

int64 QoreSocket::getConnectionId() const {
    return priv->connection_id;
}

QoreSocketTimeoutHelper::QoreSocketTimeoutHelper(QoreSocket& s, const char* op)
        : priv(new PrivateQoreSocketTimeoutHelper(qore_socket_private::get(s), op)) {
}

QoreSocketTimeoutHelper::~QoreSocketTimeoutHelper() {
    delete priv;
}

QoreSocketThroughputHelper::QoreSocketThroughputHelper(QoreSocket& s, bool snd)
        : priv(new PrivateQoreSocketThroughputHelper(qore_socket_private::get(s), snd)) {
}

QoreSocketThroughputHelper::~QoreSocketThroughputHelper() {
    delete priv;
}

void QoreSocketThroughputHelper::finalize(int64 bytes) {
    priv->finalize(bytes);
}

// --- Out-of-line implementations for public DLLEXPORT methods ---
// These were previously inline in QC_SocketPollOperation.h but are now declared
// in the public header include/qore/SocketPollOperation.h without bodies.

// SocketPollSocketOperationBase constructors/destructor
SocketPollSocketOperationBase::SocketPollSocketOperationBase(QoreObject* self)
    : SocketPollOperationBase(self) {
}

SocketPollSocketOperationBase::SocketPollSocketOperationBase(QoreSocketObject* sock)
    : sock(sock) {
}

SocketPollSocketOperationBase::SocketPollSocketOperationBase(QoreSocketObject* sock, unsigned direction)
    : sock(sock), non_block_direction(direction) {
}

SocketPollSocketOperationBase::~SocketPollSocketOperationBase() {
}

// SocketRecvPollOperationBase constructor
SocketRecvPollOperationBase::SocketRecvPollOperationBase(QoreSocketObject* sock, bool to_string)
    : SocketPollSocketOperationBase(sock, NB_RECV), to_string(to_string) {
}

void SocketPollSocketOperationBase::abort(ExceptionSink* xsink) {
    poll_state.reset();
    if (set_non_block) {
        AutoLocker al(sock->priv->m);
        sock->priv->clearNonBlock(non_block_direction);
        if (abortNeedsClose()) {
            qore_socket_close_from_controller(sock->priv->socket);
        }
        set_non_block = false;
        state = SPS_NONE;
    }
}

bool SocketPollSocketOperationBase::abortNeedsClose() const {
    return true;
}

void SocketConnectPollOperation::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        if (set_non_block) {
            sock->clearNonBlock();
        }
        // Release poll_state before deref'ing the socket: the poll state holds a raw
        // qore_socket_private* and may reference it during destruction (e.g.
        // SocketConnectInetHappyEyeballsPollState::closeAllFds reads sock->sock).
        poll_state.reset();
        sock->deref(xsink);
        delete this;
    }
}

bool SocketConnectPollOperation::goalReached() const {
    return state == SPS_CONNECTED;
}

int SocketConnectPollOperation::preVerify(ExceptionSink* xsink) {
    return 0;
}

const char* SocketConnectPollOperation::getStateImpl() const {
    switch (state) {
        case SPS_NONE: return "none";
        case SPS_CONNECTING: return "connecting";
        case SPS_CONNECTING_SSL: return "connecting-ssl";
        case SPS_CONNECTED: return "connected";
        default: assert(false);
    }
    return "";
}

void SocketSendPollOperation::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        if (set_non_block) {
            sock->clearNonBlock(NB_SEND);
        }
        poll_state.reset();
        sock->deref(xsink);
        delete this;
    }
}

bool SocketSendPollOperation::goalReached() const {
    return sent;
}

const char* SocketSendPollOperation::getStateImpl() const {
    return sent ? "sent" : "sending";
}

void SocketRecvPollOperationBase::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        if (set_non_block) {
            sock->clearNonBlock(NB_RECV);
        }
        poll_state.reset();
        sock->deref(xsink);
        delete this;
    }
}

void SocketRecvPollOperationBase::abort(ExceptionSink* xsink) {
    data.discard();
    SocketPollSocketOperationBase::abort(xsink);
}

bool SocketRecvPollOperationBase::goalReached() const {
    return received;
}

const char* SocketRecvPollOperationBase::getStateImpl() const {
    return received ? "received" : "receiving";
}

QoreValue SocketRecvPollOperationBase::getOutput() const {
    return data ? data->refSelf() : QoreValue();
}

void SocketUpgradeClientSslPollOperation::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        if (set_non_block) {
            sock->clearNonBlock();
        }
        poll_state.reset();
        sock->deref(xsink);
        delete this;
    }
}

bool SocketUpgradeClientSslPollOperation::goalReached() const {
    return done;
}

const char* SocketUpgradeClientSslPollOperation::getStateImpl() const {
    return "connecting-ssl";
}

static QoreValue qore_socket_get_ssl_upgrade_output(my_socket_priv* priv) {
    bool secure = false;
    bool is_http2 = false;
    std::string negotiated_protocol;

    {
        AutoLocker al(priv->m);
        if (priv->valid && priv->socket) {
            qore_socket_private* sp = qore_socket_private::get(*priv->socket);
            if (sp->ssl) {
                secure = true;
                negotiated_protocol = sp->ssl->getAlpnProtocol();
                is_http2 = (negotiated_protocol == "h2");
            }
        }
    }

    ExceptionSink xsink;
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), &xsink);
    h->setKeyValue("secure", secure, &xsink);
    h->setKeyValue("is_http2", is_http2, &xsink);
    if (!negotiated_protocol.empty()) {
        h->setKeyValue("negotiated_protocol", new QoreStringNode(negotiated_protocol), &xsink);
    } else {
        h->setKeyValue("negotiated_protocol", QoreValue(), &xsink);
    }
    if (xsink) {
        xsink.clear();
        return QoreValue();
    }
    return h.release();
}

QoreValue SocketUpgradeClientSslPollOperation::getOutput() const {
    return qore_socket_get_ssl_upgrade_output(sock->priv);
}

void SocketReadHttpHeaderPollOperation::abort(ExceptionSink* xsink) {
    out = nullptr;
    SocketRecvPollOperationBase::abort(xsink);
}

// --- Factory functions for public API (AsyncCompletionAction.h) ---

#include "qore/intern/AsyncCompletionAction.h"
#include "qore/intern/QC_Channel.h"
#include "qore/intern/QC_Promise.h"
#include "qore/intern/QoreEventNotifier.h"

AbstractAsyncAction* createChannelAction(QoreObject* channel_obj, ExceptionSink* xsink) {
    QoreChannel* ch = static_cast<QoreChannel*>(
        const_cast<QoreObject*>(channel_obj)->getReferencedPrivateData(CID_CHANNEL, xsink));
    if (!ch) {
        if (!*xsink) {
            xsink->raiseException("CHANNEL-ERROR", "invalid Channel object");
        }
        return nullptr;
    }
    ChannelAction* action = new ChannelAction(ch);
    ch->deref(xsink);  // ChannelAction refs internally; release getReferencedPrivateData ref
    return action;
}

AbstractAsyncAction* createEventNotifierAction(QoreObject* notifier_obj, ExceptionSink* xsink) {
    QoreEventNotifier* en = static_cast<QoreEventNotifier*>(
        const_cast<QoreObject*>(notifier_obj)->getReferencedPrivateData(CID_EVENTNOTIFIER, xsink));
    if (!en) {
        if (!*xsink) {
            xsink->raiseException("EVENTNOTIFIER-ERROR", "invalid EventNotifier object");
        }
        return nullptr;
    }
    EventNotifierAction* action = new EventNotifierAction(en);
    en->deref(xsink);  // EventNotifierAction refs internally; release getReferencedPrivateData ref
    return action;
}

AbstractAsyncAction* createPromiseWithNotifierAction(QoreObject* promise_obj,
        QoreObject* notifier_obj, ExceptionSink* xsink) {
    // Extract Promise private data
    QorePromise* promise = static_cast<QorePromise*>(
        const_cast<QoreObject*>(promise_obj)->getReferencedPrivateData(CID_PROMISE, xsink));
    if (!promise) {
        if (!*xsink) {
            xsink->raiseException("PROMISE-ERROR", "invalid Promise object");
        }
        return nullptr;
    }

    // Extract EventNotifier private data
    QoreEventNotifier* en = static_cast<QoreEventNotifier*>(
        const_cast<QoreObject*>(notifier_obj)->getReferencedPrivateData(CID_EVENTNOTIFIER, xsink));
    if (!en) {
        promise->deref(xsink);
        if (!*xsink) {
            xsink->raiseException("EVENTNOTIFIER-ERROR", "invalid EventNotifier object");
        }
        return nullptr;
    }

    // Build CompositeAction: PromiseAction resolves first, then EventNotifierAction wakes poll loop
    CompositeAction* composite = new CompositeAction();
    PromiseAction* pa = new PromiseAction(promise, promise_obj);
    promise->deref(xsink);  // PromiseAction refs internally
    EventNotifierAction* ena = new EventNotifierAction(en);
    en->deref(xsink);       // EventNotifierAction refs internally
    composite->add(pa);
    pa->deref(xsink);       // composite holds ref
    composite->add(ena);
    ena->deref(xsink);      // composite holds ref
    return composite;
}

// --- End of out-of-line implementations ---

SocketConnectPollOperation::SocketConnectPollOperation(ExceptionSink* xsink, bool ssl, const char* target,
        QoreSocketObject* sock) : SocketPollSocketOperationBase(sock), target(target),
            sandbox_manager(qore_socket_ref_current_sandbox_manager()) {
    init(xsink, ssl);
}

SocketConnectPollOperation::SocketConnectPollOperation(ExceptionSink* xsink, bool ssl, const char* target,
        QoreSocketObject* sock, bool defer_init) : SocketPollSocketOperationBase(sock), target(target),
            sandbox_manager(qore_socket_ref_current_sandbox_manager()) {
    init(xsink, ssl, defer_init);
}

SocketConnectPollOperation::SocketConnectPollOperation(ExceptionSink* xsink, bool ssl, const char* host,
        const char* service, int family, int socktype, int protocol, QoreSocketObject* sock)
        : SocketPollSocketOperationBase(sock), target(host), service(service), connect_target(ConnectTarget::Inet),
            family(family), socktype(socktype), protocol(protocol),
            sandbox_manager(qore_socket_ref_current_sandbox_manager()) {
    init(xsink, ssl);
}

SocketConnectPollOperation::SocketConnectPollOperation(ExceptionSink* xsink, bool ssl, const char* host,
        const char* service, int family, int socktype, int protocol, QoreSocketObject* sock, bool defer_init)
        : SocketPollSocketOperationBase(sock), target(host), service(service), connect_target(ConnectTarget::Inet),
            family(family), socktype(socktype), protocol(protocol),
            sandbox_manager(qore_socket_ref_current_sandbox_manager()) {
    init(xsink, ssl, defer_init);
}

SocketConnectPollOperation::SocketConnectPollOperation(ExceptionSink* xsink, bool ssl, const char* path,
        int socktype, int protocol, QoreSocketObject* sock)
        : SocketPollSocketOperationBase(sock), target(path), connect_target(ConnectTarget::Unix),
            socktype(socktype), protocol(protocol), sandbox_manager(qore_socket_ref_current_sandbox_manager()) {
    init(xsink, ssl);
}

SocketConnectPollOperation::SocketConnectPollOperation(ExceptionSink* xsink, bool ssl, const char* path,
        int socktype, int protocol, QoreSocketObject* sock, bool defer_init)
        : SocketPollSocketOperationBase(sock), target(path), connect_target(ConnectTarget::Unix),
            socktype(socktype), protocol(protocol), sandbox_manager(qore_socket_ref_current_sandbox_manager()) {
    init(xsink, ssl, defer_init);
}

void SocketConnectPollOperation::init(ExceptionSink* xsink, bool ssl) {
    init(xsink, ssl, false);
}

void SocketConnectPollOperation::init(ExceptionSink* xsink, bool ssl, bool defer_init) {
    sgoal = ssl ? SPG_CONNECT_SSL : SPG_CONNECT;
    controller_deferred_init = defer_init;
    controller_deferred_tid = defer_init ? q_gettid() : -1;
    if (defer_init) {
        return;
    }

    AutoLocker al(sock->priv->m);
    initLocked(xsink);
}

void SocketConnectPollOperation::initLocked(ExceptionSink* xsink) {
    // socket lock must be held here
    assert(sock->priv->m.trylock());
    if (initialized) {
        return;
    }
    initialized = true;

    // throw an exception and exit if the object is no longer valid
    if (sock->priv->checkValid(xsink)) {
        return;
    }

    if (preVerify(xsink)) {
        return;
    }
    int rc = controller_deferred_init
        ? sock->priv->setNonBlockFromAsyncController(xsink, NB_ALL, controller_deferred_tid)
        : sock->priv->setNonBlock(xsink);
    if (!rc) {
        set_non_block = true;
        std::string trace_target = getTraceTarget();
        ASYNC_IO_TRACE("SocketConnectPoll: startConnect target='%s' ssl=%d\n",
            trace_target.c_str(), (int)(sgoal == SPG_CONNECT_SSL));
        poll_state.reset(startConnect(xsink));
        if (!*xsink) {
            if (poll_state) {
                ASYNC_IO_TRACE("SocketConnectPoll: connect EINPROGRESS target='%s'\n", trace_target.c_str());
                state = SPS_CONNECTING;
            } else {
                ASYNC_IO_TRACE("SocketConnectPoll: connect IMMEDIATE target='%s'\n", trace_target.c_str());
                if (sgoal == SPG_CONNECT) {
                    sock->priv->clearNonBlock();
                    set_non_block = false;
                    connected();
                } else {
                    assert(sgoal == SPG_CONNECT_SSL);
                    startSslConnect(xsink);
                }
            }
        } else {
            ASYNC_IO_TRACE("SocketConnectPoll: startConnect FAILED target='%s'\n", trace_target.c_str());
        }
        if (*xsink) {
            sock->priv->clearNonBlock();
            set_non_block = false;
        }
    }
}

AbstractPollState* SocketConnectPollOperation::startConnect(ExceptionSink* xsink) {
    qore_socket_private* socket_priv = qore_socket_private::get(*sock->priv->socket);

    switch (connect_target) {
        case ConnectTarget::Auto:
            if (const char* p = strrchr(target.c_str(), ':')) {
                QoreString host(target.c_str(), p - target.c_str());
                QoreString service(p + 1);
                if (host.strlen() > 2 && host[0] == '[' && host[host.strlen() - 1] == ']') {
                    host.terminate(host.strlen() - 1);
                    return new SocketConnectInetHappyEyeballsPollState(xsink, socket_priv, host.c_str() + 1,
                        service.c_str(), AF_INET6, SOCK_STREAM, 0, *sandbox_manager);
                }
                return new SocketConnectInetHappyEyeballsPollState(xsink, socket_priv, host.c_str(),
                    service.c_str(), AF_UNSPEC, SOCK_STREAM, 0, *sandbox_manager);
            }
#ifndef _Q_WINDOWS
            return new SocketConnectUnixPollState(xsink, socket_priv, target.c_str(), SOCK_STREAM, 0,
                *sandbox_manager);
#else
            missing_function_error("Socket::startConnect(<UNIX socket file>)", "UNIX_FILEMGT", xsink);
            return nullptr;
#endif
        case ConnectTarget::Inet:
            return new SocketConnectInetHappyEyeballsPollState(xsink, socket_priv, target.c_str(), service.c_str(),
                family, socktype, protocol, *sandbox_manager);
        case ConnectTarget::Unix:
#ifndef _Q_WINDOWS
            return new SocketConnectUnixPollState(xsink, socket_priv, target.c_str(), socktype, protocol,
                *sandbox_manager);
#else
            missing_function_error("Socket::startConnect(<UNIX socket file>)", "UNIX_FILEMGT", xsink);
            return nullptr;
#endif
        default:
            assert(false);
    }
    return nullptr;
}

std::string SocketConnectPollOperation::getTraceTarget() const {
    if (connect_target == ConnectTarget::Inet) {
        std::string rv = target;
        rv += ':';
        rv += service;
        return rv;
    }
    return target;
}

QoreHashNode* SocketConnectPollOperation::continuePoll(ExceptionSink* xsink) {
    QoreHashNode* rv = nullptr;

    AutoLocker al(sock->priv->m);

    if (!initialized) {
        initLocked(xsink);
        if (*xsink || state == SPS_CONNECTED || state == SPS_NONE) {
            return nullptr;
        }
    }

    if (state == SPS_CONNECTED) {
        // throw an exception and exit if the object is no longer valid
        if (sock->priv->checkValid(xsink)) {
            return nullptr;
        }
    } else {
        auto* he_state = state == SPS_CONNECTING
            ? dynamic_cast<SocketConnectInetHappyEyeballsPollState*>(poll_state.get())
            : nullptr;
        bool resolving = he_state && he_state->getState() == HEBS_RESOLVING;
        // During async DNS resolution there is not yet a primary socket fd.
        // Once resolution finishes, connect opens the first nonblocking fd and
        // the normal open check applies again.
        int rc = resolving ? sock->priv->checkValid(xsink) : sock->priv->checkOpen(xsink);
        if (rc) {
            return nullptr;
        }
    }

    switch (state) {
        case SPS_CONNECTING: {
            int rc = checkContinuePoll(xsink);
            if (rc != 0) {
                if (*xsink) {
                    break;
                }
                // Happy Eyeballs can be waiting on c-ares resolver fds before a
                // primary socket exists, or on extra racing connect fds.
                auto* he_state = dynamic_cast<SocketConnectInetHappyEyeballsPollState*>(poll_state.get());
                if (he_state && (he_state->getState() == HEBS_RESOLVING
                        || (he_state->isRacing() && he_state->getState() == HEBS_RACING))) {
                    std::vector<std::pair<int, int>> extra_fds;
                    he_state->getExtraFds(extra_fds);
                    bumpFdGeneration();
                    rv = getSocketPollInfoHash(xsink, he_state->getPrimaryPollEvents(rc), extra_fds);
                    if (rv) {
                        int poll_timeout_ms = he_state->getPollTimeoutMs();
                        if (poll_timeout_ms >= 0) {
                            rv->setKeyValue("poll_timeout_ms", poll_timeout_ms, xsink);
                        }
                    }
                } else {
                    rv = getSocketPollInfoHash(xsink, rc);
                }
                break;
            }

            // if we are just connecting, we are done
            if (sgoal == SPG_CONNECT) {
                // SPS_CONNECTED set below
                break;
            }

            assert(sgoal == SPG_CONNECT_SSL);

            if (startSslConnect(xsink)) {
                break;
            }
        }
        // fall down to next case

        case SPS_CONNECTING_SSL: {
            int rc = checkContinuePoll(xsink);
            if (rc != 0) {
                rv = *xsink ? nullptr : getSocketPollInfoHash(xsink, rc);
                break;
            }

            // SPS_CONNECTED set below
            break;
        }

        case SPS_CONNECTED: {
            break;
        }

        case SPS_NONE: {
            // aborted
            break;
        }

        default:
            assert(false);
    }

    if (!rv) {
        if (*xsink) {
            state = SPS_NONE;
        } else {
            connected();
        }
        sock->priv->clearNonBlock();
    } else {
        assert(!*xsink);
    }
    return rv;
}

void SocketConnectPollOperation::connected() {
    // socket lock must be held here
    assert(sock->priv->m.trylock());
    state = SPS_CONNECTED;
}

int SocketConnectPollOperation::startSslConnect(ExceptionSink* xsink) {
    // socket lock must be held here
    assert(sock->priv->m.trylock());

    state = SPS_CONNECTING_SSL;

    poll_state.reset(sock->priv->socket->startSslConnect(xsink, sock->priv->cert, sock->priv->pk));
    if (*xsink) {
        poll_state.reset();
        state = SPS_NONE;
        return -1;
    }
    return 0;
}

int SocketConnectPollOperation::checkContinuePoll(ExceptionSink* xsink) {
    // socket lock must be held here
    assert(sock->priv->m.trylock());
    assert(poll_state.get());

    // see if we are able to continue
    int rc = poll_state->continuePoll(xsink);
    //printd(5, "SocketConnectPollOperation::continuePoll() state: %s rc: %d (exp: %d)\n", getStateImpl(), rc,
    //    (int)*xsink);
    if (*xsink) {
        assert(rc < 0);
        state = SPS_NONE;
        return -1;
    }
    if (!rc) {
        // release the AbstractPollState value
        poll_state.reset();
    }
    return rc;
}

SocketAcceptPollOperation::SocketAcceptPollOperation(ExceptionSink* xsink, QoreSocketObject* sock)
        : SocketAcceptPollOperation(xsink, sock, sock->priv->cert && sock->priv->pk) {
}

SocketAcceptPollOperation::SocketAcceptPollOperation(ExceptionSink* xsink, QoreSocketObject* sock, bool ssl)
        : SocketAcceptPollOperation(xsink, sock, ssl, nullptr, false) {
}

SocketAcceptPollOperation::SocketAcceptPollOperation(ExceptionSink* xsink, QoreSocketObject* sock, bool ssl,
        bool defer_init) : SocketAcceptPollOperation(xsink, sock, ssl, nullptr, defer_init) {
}

SocketAcceptPollOperation::SocketAcceptPollOperation(ExceptionSink* xsink, QoreSocketObject* sock, bool ssl,
        SocketSource* source) : SocketAcceptPollOperation(xsink, sock, ssl, source, false) {
}

SocketAcceptPollOperation::SocketAcceptPollOperation(ExceptionSink* xsink, QoreSocketObject* sock, bool ssl,
        SocketSource* source, bool defer_init) : SocketAcceptPollSocketOperationBase(sock), accepted_socket_obj(xsink),
        source(source) {
    sgoal = ssl ? SPG_ACCEPT_SSL : SPG_ACCEPT;
    init(xsink, defer_init);
}

void SocketAcceptPollOperation::init(ExceptionSink* xsink, bool defer_init) {
    if (defer_init) {
        controller_deferred_init = true;
        controller_deferred_tid = q_gettid();
        return;
    }

    AutoLocker al(sock->priv->m);
    initLocked(xsink);
}

void SocketAcceptPollOperation::initLocked(ExceptionSink* xsink) {
    assert(sock->priv->m.trylock());
    if (initialized) {
        return;
    }
    // throw an exception and exit if the object is no longer valid
    if (sock->priv->checkValid(xsink)) {
        return;
    }

    if (preVerify(xsink)) {
        return;
    }
    int rc = controller_deferred_init
        ? sock->priv->setNonBlockAcceptFromAsyncController(xsink, controller_deferred_tid)
        : sock->priv->setNonBlockAccept(xsink);
    if (!rc) {
        set_non_block_accept = true;
        poll_state.reset(new SocketAcceptPollState(xsink, sock->priv->socket->priv, source));
        if (!*xsink) {
            assert(poll_state);
            state = SPS_ACCEPTING;
        }
        if (*xsink) {
            sock->priv->clearNonBlockAccept();
            set_non_block_accept = false;
        }
    }
    if (!*xsink && poll_state) {
        initialized = true;
    }
}

QoreHashNode* SocketAcceptPollOperation::continuePoll(ExceptionSink* xsink) {
    QoreHashNode* rv = nullptr;

    AutoLocker al(sock->priv->m);

    if (!initialized) {
        initLocked(xsink);
        if (*xsink || !initialized) {
            return nullptr;
        }
    }

    if (state == SPS_ACCEPTED) {
        // throw an exception and exit if the object is no longer valid
        if (sock->priv->checkValid(xsink)) {
            return nullptr;
        }
    } else {
        // throw an exception and exit if the object is no longer open or valid
        if (sock->priv->checkOpen(xsink)) {
            return nullptr;
        }
    }

    switch (state) {
        case SPS_ACCEPTING: {
            int rc = checkContinuePoll(xsink);
            if (rc != 0) {
                rv = *xsink ? nullptr : getSocketPollInfoHash(xsink, rc);
                break;
            }

            // if we are just accepting, we are done
            if (sgoal == SPG_ACCEPT) {
                // SPS_ACCEPTED set below
                break;
            }

            assert(sgoal == SPG_ACCEPT_SSL);

            if (startSslAccept(xsink)) {
                break;
            }
        }
        // fall down to next case

        case SPS_ACCEPTING_SSL: {
            int rc = checkContinuePoll(xsink);
            if (rc != 0) {
                rv = *xsink ? nullptr : getSocketPollInfoHash(xsink, rc);
                break;
            }

            // SPS_ACCEPTED set below
            break;
        }

        case SPS_ACCEPTED: {
            break;
        }

        case SPS_NONE: {
            // aborted
            break;
        }

        default:
            assert(false);
    }

    if (!rv) {
        if (*xsink) {
            state = SPS_NONE;
        } else {
            accepted();
        }
        sock->priv->clearNonBlockAccept();
        set_non_block_accept = false;
    } else {
        assert(!*xsink);
    }
    return rv;
}

void SocketAcceptPollOperation::accepted() {
    // socket lock must be held here
    assert(sock->priv->m.trylock());
    state = SPS_ACCEPTED;
}

int SocketAcceptPollOperation::startSslAccept(ExceptionSink* xsink) {
    // socket lock must be held here
    assert(sock->priv->m.trylock());

    state = SPS_ACCEPTING_SSL;
    // The poll socket switches from the listening socket to the accepted client socket for the TLS handshake.  The
    // controller caches registrations by event mask, so force a full registration update even if both phases wait
    // for the same events.
    bumpFdGeneration();

    assert(accepted_socket);
    // we have the original socket locked, but do the SSL accept operation on the new socket without any locks,
    // however, since no one else can access it yet, this is safe
    poll_state.reset(accepted_socket->priv->socket->startSslAccept(xsink, accepted_socket->priv->cert,
            accepted_socket->priv->pk));
    if (*xsink) {
        poll_state.reset();
        state = SPS_NONE;
        return -1;
    }
    return 0;
}

int SocketAcceptPollOperation::checkContinuePoll(ExceptionSink* xsink) {
    // socket lock must be held here
    assert(sock->priv->m.trylock());
    assert(poll_state.get());

    // see if we are able to continue
    int rc = poll_state->continuePoll(xsink);
    //printd(5, "SocketAcceptPollOperation::continuePoll() state: %s rc: %d (exp: %d)\n", getStateImpl(), rc,
    //    (int)*xsink);
    if (*xsink) {
        assert(rc < 0);
        state = SPS_NONE;
        return -1;
    }
    if (!rc) {
        if (state == SPS_ACCEPTING) {
            // save socket info for getOutput() value
            assert(dynamic_cast<SocketAcceptPollState*>(poll_state.get()));
            int descriptor = reinterpret_cast<SocketAcceptPollState*>(poll_state.get())->getDescriptor();
            accepted_socket = new QoreSocketObject(*sock, descriptor);
        }
        // release the AbstractPollState value
        poll_state.reset();
    }
    return rc;
}

QoreValue SocketAcceptPollOperation::getOutput() const {
    AutoLocker al(sock->priv->m);
    // If we have a cached QoreObject wrapper (from SSL handshake polling), return that
    if (accepted_socket_obj) {
        // Release the cached object and clear accepted_socket to avoid double-free
        accepted_socket.discard();
        return accepted_socket_obj.release();
    }
    if (accepted_socket) {
        return new QoreObject(QC_SOCKET, getProgram(), accepted_socket.release());
    }
    return QoreValue();
}

SocketUpgradeClientSslPollOperation::SocketUpgradeClientSslPollOperation(ExceptionSink* xsink, QoreSocketObject* sock)
        : SocketPollSocketOperationBase(sock) {
    init(xsink, false);
}

SocketUpgradeClientSslPollOperation::SocketUpgradeClientSslPollOperation(ExceptionSink* xsink, QoreSocketObject* sock,
        bool defer_init) : SocketPollSocketOperationBase(sock) {
    init(xsink, defer_init);
}

void SocketUpgradeClientSslPollOperation::init(ExceptionSink* xsink, bool defer_init) {
    controller_deferred_init = defer_init;
    controller_deferred_tid = defer_init ? q_gettid() : -1;
    if (defer_init) {
        return;
    }

    AutoLocker al(sock->priv->m);
    initLocked(xsink);
}

void SocketUpgradeClientSslPollOperation::initLocked(ExceptionSink* xsink) {
    // socket lock must be held here
    assert(sock->priv->m.trylock());
    if (initialized) {
        return;
    }
    initialized = true;

    // Throw an exception and exit if the object is no longer open and valid. If SSL is already active, then the
    // upgrade is already complete.
    if (sock->priv->checkOpen(xsink)) {
        return;
    }
    if (sock->priv->socket->isSecure()) {
        done = true;
        return;
    }

    int rc = controller_deferred_init
        ? sock->priv->setNonBlockFromAsyncController(xsink, NB_ALL, controller_deferred_tid)
        : sock->priv->setNonBlock(xsink);
    if (!rc) {
        set_non_block = true;

        poll_state.reset(sock->priv->socket->startSslConnect(xsink, sock->priv->cert, sock->priv->pk));
        if (*xsink) {
            sock->priv->clearNonBlock();
            set_non_block = false;
        }
    }
}

QoreHashNode* SocketUpgradeClientSslPollOperation::continuePoll(ExceptionSink* xsink) {
    AutoLocker al(sock->priv->m);

    if (done) {
        return nullptr;
    }

    if (!initialized) {
        initLocked(xsink);
        if (*xsink || done) {
            return nullptr;
        }
    }

    // throw an exception and exit if the object is no longer open and valid
    if (sock->priv->checkOpen(xsink)) {
        return nullptr;
    }

    assert(poll_state);

    // see if we are able to continue
    int rc = poll_state->continuePoll(xsink);
    //printd(5, "SocketUpgradeClientSslPollOperation::continuePoll() state: %s rc: %d (exp: %d)\n", getStateImpl(),
    //    rc, (int)*xsink);
    if (*xsink) {
        assert(rc < 0);
        return nullptr;
    }
    if (!rc) {
        // release the AbstractPollState value
        poll_state.reset();
        sock->priv->clearNonBlock();
        set_non_block = false;
        done = true;
        return nullptr;
    }

    return getSocketPollInfoHash(xsink, rc);
}

SocketUpgradeServerSslPollOperation::SocketUpgradeServerSslPollOperation(ExceptionSink* xsink, QoreSocketObject* sock)
        : SocketPollSocketOperationBase(sock) {
    init(xsink, false);
}

SocketUpgradeServerSslPollOperation::SocketUpgradeServerSslPollOperation(ExceptionSink* xsink, QoreSocketObject* sock,
        bool defer_init) : SocketPollSocketOperationBase(sock) {
    init(xsink, defer_init);
}

void SocketUpgradeServerSslPollOperation::init(ExceptionSink* xsink, bool defer_init) {
    controller_deferred_init = defer_init;
    controller_deferred_tid = defer_init ? q_gettid() : -1;
    if (defer_init) {
        return;
    }

    AutoLocker al(sock->priv->m);
    initLocked(xsink);
}

void SocketUpgradeServerSslPollOperation::initLocked(ExceptionSink* xsink) {
    // socket lock must be held here
    assert(sock->priv->m.trylock());
    if (initialized) {
        return;
    }
    initialized = true;

    // Throw an exception and exit if the object is no longer open and valid. If SSL is already active, then the
    // upgrade is already complete.
    if (sock->priv->checkOpen(xsink)) {
        return;
    }
    if (sock->priv->socket->isSecure()) {
        done = true;
        return;
    }

    int rc = controller_deferred_init
        ? sock->priv->setNonBlockFromAsyncController(xsink, NB_ALL, controller_deferred_tid)
        : sock->priv->setNonBlock(xsink);
    if (!rc) {
        set_non_block = true;

        poll_state.reset(sock->priv->socket->startSslAccept(xsink, sock->priv->cert, sock->priv->pk));
        if (*xsink) {
            poll_state.reset();
            sock->priv->clearNonBlock();
            set_non_block = false;
        }
    }
}

QoreHashNode* SocketUpgradeServerSslPollOperation::continuePoll(ExceptionSink* xsink) {
    AutoLocker al(sock->priv->m);

    if (done) {
        return nullptr;
    }

    if (!initialized) {
        initLocked(xsink);
        if (*xsink || done) {
            return nullptr;
        }
    }

    // throw an exception and exit if the object is no longer open and valid
    if (sock->priv->checkOpen(xsink)) {
        return nullptr;
    }

    assert(poll_state);

    // see if we are able to continue
    int rc = poll_state->continuePoll(xsink);
    //printd(5, "SocketUpgradeServerSslPollOperation::continuePoll() state: %s rc: %d (exp: %d)\n", getStateImpl(),
    //    rc, (int)*xsink);
    if (*xsink) {
        assert(rc < 0);
        return nullptr;
    }
    if (!rc) {
        // release the AbstractPollState value
        poll_state.reset();
        sock->priv->clearNonBlock();
        set_non_block = false;
        done = true;
        return nullptr;
    }

    return getSocketPollInfoHash(xsink, rc);
}

QoreValue SocketUpgradeServerSslPollOperation::getOutput() const {
    return qore_socket_get_ssl_upgrade_output(sock->priv);
}

SocketShutdownSslPollOperation::SocketShutdownSslPollOperation(ExceptionSink* xsink, QoreSocketObject* sock)
        : SocketPollSocketOperationBase(sock) {
    init(xsink, false);
}

SocketShutdownSslPollOperation::SocketShutdownSslPollOperation(ExceptionSink* xsink, QoreSocketObject* sock,
        bool defer_init) : SocketPollSocketOperationBase(sock) {
    init(xsink, defer_init);
}

void SocketShutdownSslPollOperation::init(ExceptionSink* xsink, bool defer_init) {
    controller_deferred_init = defer_init;
    controller_deferred_tid = defer_init ? q_gettid() : -1;
    if (defer_init) {
        return;
    }

    AutoLocker al(sock->priv->m);
    initLocked(xsink);
}

void SocketShutdownSslPollOperation::initLocked(ExceptionSink* xsink) {
    // socket lock must be held here
    assert(sock->priv->m.trylock());
    if (initialized) {
        return;
    }
    initialized = true;

    if (sock->priv->checkValid(xsink)) {
        return;
    }
    if (!sock->priv->socket->isOpen() || !sock->priv->socket->isSecure()) {
        done = true;
        return;
    }

    int rc = controller_deferred_init
        ? sock->priv->setNonBlockFromAsyncController(xsink, NB_ALL, controller_deferred_tid)
        : sock->priv->setNonBlock(xsink);
    if (!rc) {
        set_non_block = true;
        poll_state.reset(new SocketShutdownSslPollState(xsink, qore_socket_private::get(*sock->priv->socket)));
        if (*xsink) {
            poll_state.reset();
            sock->priv->clearNonBlock();
            set_non_block = false;
        }
    }
}

QoreHashNode* SocketShutdownSslPollOperation::continuePoll(ExceptionSink* xsink) {
    AutoLocker al(sock->priv->m);

    if (done) {
        return nullptr;
    }

    if (!initialized) {
        initLocked(xsink);
        if (*xsink || done) {
            return nullptr;
        }
    }

    if (sock->priv->checkValid(xsink)) {
        return nullptr;
    }
    if (!sock->priv->socket->isOpen() || !sock->priv->socket->isSecure()) {
        if (set_non_block) {
            sock->priv->clearNonBlock();
            set_non_block = false;
        }
        done = true;
        return nullptr;
    }

    assert(poll_state);
    int rc = poll_state->continuePoll(xsink);
    if (*xsink) {
        assert(rc < 0);
        return nullptr;
    }
    if (!rc) {
        poll_state.reset();
        if (set_non_block) {
            sock->priv->clearNonBlock();
            set_non_block = false;
        }
        done = true;
        return nullptr;
    }

    return getSocketPollInfoHash(xsink, rc);
}

SocketSetupPollOperation::SocketSetupPollOperation(ExceptionSink* xsink, QoreSocketObject* sock, const char* name,
        bool reuseaddr) : SocketPollSocketOperationBase(sock), action(Action::BindUnix), name(name),
        has_name(true), reuseaddr(reuseaddr), socktype(SOCK_STREAM) {
    if (qore_socket_parse_bind_name(name, this->name, service, family)) {
        action = Action::BindInet;
        has_service = true;
    }
    init(xsink, true);
}

SocketSetupPollOperation::SocketSetupPollOperation(ExceptionSink* xsink, QoreSocketObject* sock, int port,
        bool reuseaddr) : SocketPollSocketOperationBase(sock), action(Action::BindPort), reuseaddr(reuseaddr),
        port(port) {
    service = std::to_string(port);
    has_service = true;
    socktype = SOCK_STREAM;
    init(xsink, true);
}

SocketSetupPollOperation::SocketSetupPollOperation(ExceptionSink* xsink, QoreSocketObject* sock, const char* iface,
        int port, bool reuseaddr) : SocketPollSocketOperationBase(sock), action(Action::BindInterfacePort),
        name(iface), has_name(true), reuseaddr(reuseaddr), port(port) {
    service = std::to_string(port);
    has_service = true;
    socktype = SOCK_STREAM;
    init(xsink, true);
}

SocketSetupPollOperation::SocketSetupPollOperation(ExceptionSink* xsink, QoreSocketObject* sock, const char* name,
        int socktype, int protocol) : SocketPollSocketOperationBase(sock), action(Action::BindUnix), name(name),
        has_name(true), socktype(socktype), protocol(protocol) {
    init(xsink, true);
}

SocketSetupPollOperation::SocketSetupPollOperation(ExceptionSink* xsink, QoreSocketObject* sock, const char* name,
        const char* service, bool reuseaddr, int family, int socktype, int protocol)
        : SocketPollSocketOperationBase(sock), action(Action::BindInet), reuseaddr(reuseaddr), family(family),
        socktype(socktype), protocol(protocol) {
    if (name) {
        this->name = name;
        has_name = true;
    }
    if (service) {
        this->service = service;
        has_service = true;
    }
    init(xsink, true);
}

SocketSetupPollOperation::SocketSetupPollOperation(ExceptionSink* xsink, QoreSocketObject* sock, int backlog)
        : SocketPollSocketOperationBase(sock), action(Action::Listen), backlog(backlog) {
    init(xsink, true);
}

SocketSetupPollOperation::SocketSetupPollOperation(ExceptionSink* xsink, QoreSocketObject* sock,
        ConfigAction config_action, int64 value)
        : SocketPollSocketOperationBase(sock), action(getAction(config_action)), value(value) {
    init(xsink, true);
}

SocketSetupPollOperation::Action SocketSetupPollOperation::getAction(ConfigAction config_action) {
    switch (config_action) {
        case ConfigAction::SetNoDelay:
            return Action::SetNoDelay;
        case ConfigAction::GetNoDelay:
            return Action::GetNoDelay;
        case ConfigAction::SetUserTimeout:
            return Action::SetUserTimeout;
        case ConfigAction::GetUserTimeout:
            return Action::GetUserTimeout;
        case ConfigAction::SetSendTimeout:
            return Action::SetSendTimeout;
        case ConfigAction::SetRecvTimeout:
            return Action::SetRecvTimeout;
        case ConfigAction::GetSendTimeout:
            return Action::GetSendTimeout;
        case ConfigAction::GetRecvTimeout:
            return Action::GetRecvTimeout;
        case ConfigAction::GetPort:
            return Action::GetPort;
        case ConfigAction::SetMaxChunkedBodySize:
            return Action::SetMaxChunkedBodySize;
        case ConfigAction::SetSslVerifyMode:
            return Action::SetSslVerifyMode;
        case ConfigAction::GetSslVerifyMode:
            return Action::GetSslVerifyMode;
        case ConfigAction::SetAcceptAllCertificates:
            return Action::SetAcceptAllCertificates;
        case ConfigAction::GetAcceptAllCertificates:
            return Action::GetAcceptAllCertificates;
        case ConfigAction::CaptureRemoteCertificates:
            return Action::CaptureRemoteCertificates;
        case ConfigAction::SetHttp2MaxRequestBodySize:
            return Action::SetHttp2MaxRequestBodySize;
    }
    assert(false);
    return Action::GetNoDelay;
}

bool SocketSetupPollOperation::isConfigAction() const {
    return action == Action::SetNoDelay
        || action == Action::GetNoDelay
        || action == Action::SetUserTimeout
        || action == Action::GetUserTimeout
        || action == Action::SetSendTimeout
        || action == Action::SetRecvTimeout
        || action == Action::GetSendTimeout
        || action == Action::GetRecvTimeout
        || action == Action::GetPort
        || action == Action::SetMaxChunkedBodySize
        || action == Action::SetSslVerifyMode
        || action == Action::GetSslVerifyMode
        || action == Action::SetAcceptAllCertificates
        || action == Action::GetAcceptAllCertificates
        || action == Action::CaptureRemoteCertificates
        || action == Action::SetHttp2MaxRequestBodySize;
}

void SocketSetupPollOperation::init(ExceptionSink* xsink, bool defer_init) {
    if (defer_init) {
        controller_deferred_tid = q_gettid();
        return;
    }

    AutoLocker al(sock->priv->m);
    initLocked(xsink);
}

int SocketSetupPollOperation::initLocked(ExceptionSink* xsink) {
    assert(sock->priv->m.trylock());
    if (initialized) {
        return 0;
    }
    if (isConfigAction()) {
        initialized = true;
        return 0;
    }
    if (!sock->priv->setNonBlockFromAsyncController(xsink, NB_ALL, controller_deferred_tid)) {
        set_non_block = true;
        initialized = true;
        return 0;
    }
    return -1;
}

void SocketSetupPollOperation::clearNonBlockLocked() {
    if (set_non_block) {
        sock->priv->clearNonBlock();
        set_non_block = false;
    }
}

QoreHashNode* SocketSetupPollOperation::continuePoll(ExceptionSink* xsink) {
    AutoLocker al(sock->priv->m);

    if (done) {
        return nullptr;
    }

    if (!initialized && initLocked(xsink)) {
        return nullptr;
    }

    if (sock->priv->checkValid(xsink)) {
        clearNonBlockLocked();
        return nullptr;
    }

    switch (action) {
        case Action::BindPort:
            return continueBindInet(xsink);
        case Action::BindInterfacePort:
            return continueBindInet(xsink);
        case Action::BindUnix:
            rc = qore_socket_bind_unix_direct(sock->priv->socket, name.c_str(), socktype, protocol, xsink);
            break;
        case Action::BindInet:
            return continueBindInet(xsink);
        case Action::Listen:
            rc = qore_socket_listen_direct(sock->priv->socket, backlog);
            break;
        case Action::SetNoDelay:
            rc = qore_socket_set_no_delay_direct(sock->priv->socket, static_cast<int>(value));
            break;
        case Action::GetNoDelay:
            rc = qore_socket_get_no_delay_direct(sock->priv->socket);
            break;
        case Action::SetUserTimeout:
            rc = qore_socket_set_user_timeout_direct(sock->priv->socket, static_cast<int>(value));
            break;
        case Action::GetUserTimeout:
            rc = qore_socket_get_user_timeout_direct(sock->priv->socket);
            break;
        case Action::SetSendTimeout:
            rc = qore_socket_set_socket_timeout_direct(sock->priv->socket, SO_SNDTIMEO, static_cast<int>(value));
            break;
        case Action::SetRecvTimeout:
            rc = qore_socket_set_socket_timeout_direct(sock->priv->socket, SO_RCVTIMEO, static_cast<int>(value));
            break;
        case Action::GetSendTimeout:
            rc = qore_socket_get_socket_timeout_direct(sock->priv->socket, SO_SNDTIMEO);
            break;
        case Action::GetRecvTimeout:
            rc = qore_socket_get_socket_timeout_direct(sock->priv->socket, SO_RCVTIMEO);
            break;
        case Action::GetPort:
            rc = qore_socket_get_port_direct(sock->priv->socket);
            break;
        case Action::SetMaxChunkedBodySize: {
            qore_socket_private* sp = qore_socket_private::get(*sock->priv->socket);
            sp->max_chunked_body_size.store(value, std::memory_order_relaxed);
            rc = 0;
            break;
        }
        case Action::SetSslVerifyMode: {
            qore_socket_private* sp = qore_socket_private::get(*sock->priv->socket);
            sp->setSslVerifyMode(static_cast<int>(value));
            rc = 0;
            break;
        }
        case Action::GetSslVerifyMode: {
            qore_socket_private* sp = qore_socket_private::get(*sock->priv->socket);
            rc = sp->ssl_verify_mode;
            break;
        }
        case Action::SetAcceptAllCertificates: {
            qore_socket_private* sp = qore_socket_private::get(*sock->priv->socket);
            sp->acceptAllCertificates(value != 0);
            rc = 0;
            break;
        }
        case Action::GetAcceptAllCertificates: {
            qore_socket_private* sp = qore_socket_private::get(*sock->priv->socket);
            rc = sp->ssl_accept_all_certs ? 1 : 0;
            break;
        }
        case Action::CaptureRemoteCertificates: {
            qore_socket_private* sp = qore_socket_private::get(*sock->priv->socket);
            rc = sp->ssl_capture_remote_cert ? 1 : 0;
            sp->ssl_capture_remote_cert = value != 0;
            break;
        }
        case Action::SetHttp2MaxRequestBodySize: {
            qore_socket_private* sp = qore_socket_private::get(*sock->priv->socket);
            sp->max_http2_body_size.store(value, std::memory_order_relaxed);
            AutoLocker hal(sp->h2_session_lock);
            if (sp->h2_session) {
                sp->h2_session->setMaxRequestBodySize(value);
            }
            rc = 0;
            break;
        }
    }

    clearNonBlockLocked();
    done = true;
    return nullptr;
}

SocketDataAvailablePollOperation::SocketDataAvailablePollOperation(ExceptionSink* xsink, QoreSocketObject* sock)
        : SocketPollSocketOperationBase(sock, NB_RECV) {
    init(xsink, false);
}

SocketDataAvailablePollOperation::SocketDataAvailablePollOperation(ExceptionSink* xsink, QoreSocketObject* sock,
        bool defer_init) : SocketPollSocketOperationBase(sock, NB_RECV) {
    init(xsink, defer_init);
}

void SocketDataAvailablePollOperation::init(ExceptionSink* xsink, bool defer_init) {
    controller_deferred_init = defer_init;
    controller_deferred_tid = defer_init ? q_gettid() : -1;
    if (defer_init) {
        return;
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*sock);
    AutoLocker al(priv->m);
    initLocked(xsink);
}

int SocketDataAvailablePollOperation::initLocked(ExceptionSink* xsink) {
    my_socket_priv* priv = my_socket_priv::getPriv(*sock);
    assert(priv->m.trylock());
    if (initialized) {
        return 0;
    }
    if (priv->checkOpen(xsink)) {
        return -1;
    }
    int rc = controller_deferred_init
        ? priv->setNonBlockFromAsyncController(xsink, NB_RECV, controller_deferred_tid)
        : priv->setNonBlock(xsink, NB_RECV);
    if (!rc) {
        set_non_block = true;
        initialized = true;
        return 0;
    }
    return -1;
}

void SocketDataAvailablePollOperation::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        clearNonBlock();
        poll_state.reset();
        sock->deref(xsink);
        delete this;
    }
}

void SocketDataAvailablePollOperation::clearNonBlock() {
    if (set_non_block) {
        my_socket_priv* priv = my_socket_priv::getPriv(*sock);
        AutoLocker al(priv->m);
        if (set_non_block) {
            priv->clearNonBlock(NB_RECV);
            set_non_block = false;
        }
    }
}

bool SocketDataAvailablePollOperation::complete(bool value) {
    if (set_non_block) {
        my_socket_priv::getPriv(*sock)->clearNonBlock(NB_RECV);
        set_non_block = false;
    }
    ready = value;
    return ready;
}

QoreHashNode* SocketDataAvailablePollOperation::continuePoll(ExceptionSink* xsink) {
    my_socket_priv* priv = my_socket_priv::getPriv(*sock);
    AutoLocker al(priv->m);

    if (ready) {
        return nullptr;
    }

    if (!initialized && initLocked(xsink)) {
        complete(false);
        return nullptr;
    }

    if (priv->checkOpen(xsink)) {
        complete(false);
        return nullptr;
    }

    qore_socket_private* sp = qore_socket_private::get(*priv->socket);
    if (sp->buflen) {
        complete(true);
        return nullptr;
    }

    int32_t h2_active_stream_id = sp->getH2ActiveStreamId();
    if (sp->h2_session && sp->h2_session->isServer() && h2_active_stream_id > 0 && !sp->h2_receiving_frames) {
        xsink->raiseException("SOCKET-H2-SYNC-ERROR",
            "Socket::isDataAvailable() is not supported on an HTTP/2-active "
            "server socket; use HttpServerAsyncIo + register_body_queue "
            "for inbound DATA");
        complete(false);
        return nullptr;
    }

    if (sp->ssl) {
        char c;
        size_t real_io = 0;
        OptionalNonBlockingHelper nbh(*sp, true, xsink);
        if (*xsink) {
            complete(false);
            return nullptr;
        }
        int rc = sp->ssl->doNonBlockingIo(xsink, "isDataAvailable", &c, 1, PEEK, real_io);
        if (*xsink) {
            complete(false);
            return nullptr;
        }
        if (!rc) {
            complete(real_io > 0);
            return nullptr;
        }
        return getSocketPollInfoHash(xsink, rc);
    }

    if (qore_socket_is_accepting(sp)) {
        if (!waiting) {
            waiting = true;
            return getSocketPollInfoHash(xsink, SOCK_POLLIN);
        }

        complete(true);
        return nullptr;
    }

    int data_available = qore_socket_plain_data_available(sp, "isDataAvailable", xsink);
    if (*xsink) {
        complete(false);
        return nullptr;
    }
    if (data_available > 0) {
        complete(true);
        return nullptr;
    }

    waiting = true;
    return getSocketPollInfoHash(xsink, SOCK_POLLIN);
}

SocketSendPollOperation::SocketSendPollOperation(ExceptionSink* xsink, QoreStringNode* data, QoreSocketObject* sock)
        : SocketPollSocketOperationBase(sock, NB_SEND), data(data), buf(data->c_str()), size(data->size()) {
    init(xsink, false);
}

SocketSendPollOperation::SocketSendPollOperation(ExceptionSink* xsink, QoreStringNode* data, QoreSocketObject* sock,
        bool defer_init)
        : SocketPollSocketOperationBase(sock, NB_SEND), data(data), buf(data->c_str()), size(data->size()) {
    init(xsink, defer_init);
}

SocketSendPollOperation::SocketSendPollOperation(ExceptionSink* xsink, BinaryNode* data, QoreSocketObject* sock)
        : SocketPollSocketOperationBase(sock, NB_SEND), data(data), buf(reinterpret_cast<const char*>(data->getPtr())),
        size(data->size()) {
    init(xsink, false);
}

SocketSendPollOperation::SocketSendPollOperation(ExceptionSink* xsink, BinaryNode* data, QoreSocketObject* sock,
        bool defer_init)
        : SocketPollSocketOperationBase(sock, NB_SEND), data(data), buf(reinterpret_cast<const char*>(data->getPtr())),
        size(data->size()) {
    init(xsink, defer_init);
}

void SocketSendPollOperation::init(ExceptionSink* xsink, bool defer_init) {
    controller_deferred_init = defer_init;
    controller_deferred_tid = defer_init ? q_gettid() : -1;
    if (defer_init) {
        return;
    }

    AutoLocker al(sock->priv->m);
    initLocked(xsink);
}

int SocketSendPollOperation::initLocked(ExceptionSink* xsink) {
    assert(sock->priv->m.trylock());

    if (initialized) {
        return 0;
    }
    // throw an exception and exit if the object is no longer open or valid
    if (sock->priv->checkOpen(xsink)) {
        return -1;
    }

    int rc = controller_deferred_init
        ? sock->priv->setNonBlockFromAsyncController(xsink, NB_SEND, controller_deferred_tid)
        : sock->priv->setNonBlock(xsink, NB_SEND);
    if (!rc) {
        poll_state.reset(sock->priv->socket->startSend(xsink, buf, size));
        if (!poll_state) {
            sock->priv->clearNonBlock(NB_SEND);
        } else {
            set_non_block = true;
            initialized = true;
        }
    }
    return *xsink || !poll_state ? -1 : 0;
}

bool SocketSendPollOperation::abortNeedsClose() const {
    if (poll_state) {
        assert(dynamic_cast<SocketSendPollState*>(poll_state.get()));
        return static_cast<SocketSendPollState*>(poll_state.get())->getBytesSent() ? true : false;
    }
    return true;
}

QoreHashNode* SocketSendPollOperation::continuePoll(ExceptionSink* xsink) {
    AutoLocker al(sock->priv->m);

    if (!initialized) {
        if (initLocked(xsink)) {
            return nullptr;
        }
    } else if (sock->priv->checkOpen(xsink)) {
        return nullptr;
    }

    if (!poll_state) {
        return nullptr;
    }

    // see if we are able to continue
    int rc = poll_state->continuePoll(xsink);
    //printd(5, "SocketConnectPollOperation::continuePoll() state: %s rc: %d (exp: %d)\n", getStateImpl(), rc,
    //    (int)*xsink);
    if (*xsink || !rc) {
        // release the AbstractPollState value
        poll_state.reset();
        sock->priv->clearNonBlock(NB_SEND);
        set_non_block = false;
        if (!*xsink) {
            sent = true;
        }
        return nullptr;
    }
    return getSocketPollInfoHash(xsink, rc);
}

SocketSendInputStreamPollOperation::SocketSendInputStreamPollOperation(ExceptionSink* xsink, QoreSocketObject* sock,
        InputStream* input_stream, QoreObject* input_stream_obj, int64 size, int timeout_ms, bool emit_data_events)
        : SocketPollSocketOperationBase(sock, NB_SEND), input_stream(input_stream), input_stream_obj(input_stream_obj),
          size(size), timeout_ms(timeout_ms), emit_data_events(emit_data_events) {
    init(xsink, false);
}

SocketSendInputStreamPollOperation::SocketSendInputStreamPollOperation(ExceptionSink* xsink, QoreSocketObject* sock,
        InputStream* input_stream, QoreObject* input_stream_obj, int64 size, int timeout_ms, bool emit_data_events,
        bool defer_init)
        : SocketPollSocketOperationBase(sock, NB_SEND), input_stream(input_stream), input_stream_obj(input_stream_obj),
          size(size), timeout_ms(timeout_ms), emit_data_events(emit_data_events) {
    init(xsink, defer_init);
}

void SocketSendInputStreamPollOperation::init(ExceptionSink* xsink, bool defer_init) {
    controller_deferred_init = defer_init;
    controller_deferred_tid = defer_init ? q_gettid() : -1;
    if (!input_stream->isIoThreadSafe()) {
        xsink->raiseException("SOCKET-SEND-ERROR", "InputStream is not I/O thread safe");
        return;
    }

    stream_fd = qore_input_stream_pollable_descriptor(*input_stream, "SOCKET-SEND-ERROR", xsink);
    if (*xsink) {
        return;
    }
    is_pollable = stream_fd >= 0;

    if (defer_init) {
        return;
    }

    AutoLocker al(sock->priv->m);
    initLocked(xsink);
}

int SocketSendInputStreamPollOperation::initLocked(ExceptionSink* xsink) {
    assert(sock->priv->m.trylock());
    if (initialized) {
        return 0;
    }

    if (sock->priv->checkOpen(xsink)) {
        return -1;
    }

    int rc = controller_deferred_init
        ? sock->priv->setNonBlockFromAsyncController(xsink, NB_SEND, controller_deferred_tid)
        : sock->priv->setNonBlock(xsink, NB_SEND);
    if (!rc) {
        set_non_block = true;
        initialized = true;
        return 0;
    }
    return -1;
}

QoreHashNode* SocketSendInputStreamPollOperation::getPollInfo(ExceptionSink* xsink, int events, bool stream_wait) {
    int64 poll_timeout_ms = -1;
    if (timeout_ms >= 0) {
        int us;
        int64 now_s = q_epoch_us(us);
        int64 now_ms = now_s * 1000 + us / 1000;
        if (!wait_deadline_ms) {
            wait_deadline_ms = now_ms + timeout_ms;
        }
        poll_timeout_ms = wait_deadline_ms - now_ms;
        if (poll_timeout_ms < 0) {
            poll_timeout_ms = 0;
        }
    }

    QoreHashNode* raw_info = nullptr;
    if (stream_wait && is_pollable && stream_fd >= 0) {
        std::vector<std::pair<int, int>> extra_fds{{stream_fd, SOCK_POLLIN}};
        raw_info = getSocketPollInfoHash(xsink, 0, extra_fds);
    } else {
        raw_info = getSocketPollInfoHash(xsink, events);
    }
    if (!raw_info) {
        return nullptr;
    }
    ReferenceHolder<QoreHashNode> info(raw_info, xsink);
    if (poll_timeout_ms >= 0) {
        info->setKeyValue("poll_timeout_ms", poll_timeout_ms, xsink);
    }
    return info.release();
}

int64 SocketSendInputStreamPollOperation::getNextChunkSize() const {
    if (size < 0) {
        return DEFAULT_SOCKET_BUFSIZE;
    }
    return QORE_MIN(size - bytes_sent, (int64)DEFAULT_SOCKET_BUFSIZE);
}

void SocketSendInputStreamPollOperation::complete(ExceptionSink* xsink) {
    if (input_stream && !need_reassign) {
        input_stream->unassignThread(xsink);
        if (*xsink) {
            phase = Phase::Error;
            return;
        }
    }
    input_stream = nullptr;
    phase = Phase::Done;
    if (set_non_block) {
        sock->priv->clearNonBlock(NB_SEND);
        set_non_block = false;
    }
}

bool SocketSendInputStreamPollOperation::checkTimeout(ExceptionSink* xsink) {
    if (wait_deadline_ms <= 0) {
        return false;
    }
    int us;
    int64 now_s = q_epoch_us(us);
    int64 now_ms = now_s * 1000 + us / 1000;
    if (now_ms < wait_deadline_ms) {
        return false;
    }
    xsink->raiseException("SOCKET-TIMEOUT", "socket operation timed out");
    phase = Phase::Error;
    return true;
}

void SocketSendInputStreamPollOperation::clearTimeout() {
    wait_deadline_ms = 0;
}

QoreHashNode* SocketSendInputStreamPollOperation::continuePoll(ExceptionSink* xsink) {
    if (need_reassign) {
        need_reassign = false;
        if (input_stream) {
            input_stream->reassignThread(xsink);
            if (*xsink) {
                phase = Phase::Error;
                return nullptr;
            }
        }
    }

    AutoLocker al(sock->priv->m);
    if (!initialized && initLocked(xsink)) {
        phase = Phase::Error;
        return nullptr;
    }
    if (sock->priv->checkOpen(xsink)) {
        phase = Phase::Error;
        return nullptr;
    }
    if (checkTimeout(xsink)) {
        return nullptr;
    }

    unsigned loop = 0;
    while (true) {
        switch (phase) {
            case Phase::ReadChunk: {
                if (size >= 0 && bytes_sent >= size) {
                    complete(xsink);
                    return nullptr;
                }

                int64 chunk_size = getNextChunkSize();
                if (chunk_size <= 0) {
                    complete(xsink);
                    return nullptr;
                }

                if (is_pollable) {
                    assert(stream_fd >= 0);
                    int poll_rv = qore_socket_poll_fd_now(stream_fd, POLLIN, "SOCKET-SEND-ERROR", "stream",
                        xsink);
                    if (poll_rv < 0) {
                        phase = Phase::Error;
                        return nullptr;
                    }
                    if (poll_rv == 0) {
                        return getPollInfo(xsink, SOCK_POLLIN, true);
                    }

                    SimpleRefHolder<BinaryNode> chunk(new BinaryNode);
                    chunk->preallocate(chunk_size);
                    int64 count = input_stream->readNonBlock(
                        const_cast<void*>(chunk->getPtr()), chunk_size, xsink);
                    if (*xsink) {
                        phase = Phase::Error;
                        return nullptr;
                    }
                    if (count <= 0) {
                        if (size >= 0) {
                            xsink->raiseException("SOCKET-SEND-ERROR", "Unexpected end of stream");
                            phase = Phase::Error;
                            return nullptr;
                        }
                        complete(xsink);
                        return nullptr;
                    }
                    chunk->setSize(count);
                    current_chunk = chunk.release();
                    clearTimeout();
                } else {
                    current_chunk = input_stream->readHelper(chunk_size, xsink);
                    if (*xsink) {
                        phase = Phase::Error;
                        return nullptr;
                    }
                    if (!current_chunk) {
                        if (size >= 0) {
                            xsink->raiseException("SOCKET-SEND-ERROR", "Unexpected end of stream");
                            phase = Phase::Error;
                            return nullptr;
                        }
                        complete(xsink);
                        return nullptr;
                    }
                    clearTimeout();
                }

                phase = Phase::SendChunk;
                continue;
            }

            case Phase::SendChunk: {
                if (!poll_state) {
                    poll_state.reset(sock->priv->socket->startSend(xsink,
                        reinterpret_cast<const char*>(current_chunk->getPtr()), current_chunk->size()));
                    if (*xsink || !poll_state) {
                        phase = Phase::Error;
                        return nullptr;
                    }
                }

                SocketSendPollState* send_state = static_cast<SocketSendPollState*>(poll_state.get());
                if (send_state->getBytesSent()) {
                    socket_data_sent = true;
                }
                size_t sent_before = send_state->getBytesSent();
                int rc = poll_state->continuePoll(xsink);
                if (*xsink) {
                    phase = Phase::Error;
                    poll_state.reset();
                    return nullptr;
                }
                if (send_state->getBytesSent() > sent_before) {
                    socket_data_sent = true;
                    clearTimeout();
                }
                if (rc) {
                    return getPollInfo(xsink, rc);
                }

                poll_state.reset();
                bytes_sent += current_chunk->size();
                if (emit_data_events) {
                    qore_socket_private::get(*sock->priv->socket)->do_data_event(
                        QORE_EVENT_SOCKET_DATA_SENT, QORE_SOURCE_SOCKET, **current_chunk);
                }
                current_chunk = nullptr;
                if (++loop >= max_nonblock_ops && (size < 0 || bytes_sent < size)) {
                    return getPollInfo(xsink, SOCK_POLLOUT);
                }
                phase = Phase::ReadChunk;
                continue;
            }

            case Phase::Done:
            case Phase::Error:
                return nullptr;
        }
    }
}

SocketSendHttpChunkedInputStreamPollOperation::SocketSendHttpChunkedInputStreamPollOperation(ExceptionSink* xsink,
        QoreSocketObject* sock, InputStream* input_stream, QoreObject* input_stream_obj, size_t max_chunk_size,
        int timeout_ms, bool send_terminal_chunk, int source)
        : SocketPollSocketOperationBase(sock, NB_SEND), input_stream(input_stream), input_stream_obj(input_stream_obj),
          max_chunk_size(max_chunk_size), timeout_ms(timeout_ms), send_terminal_chunk(send_terminal_chunk),
          source(source) {
    init(xsink, false);
}

SocketSendHttpChunkedInputStreamPollOperation::SocketSendHttpChunkedInputStreamPollOperation(ExceptionSink* xsink,
        QoreSocketObject* sock, InputStream* input_stream, QoreObject* input_stream_obj, size_t max_chunk_size,
        int timeout_ms, bool send_terminal_chunk, int source, bool defer_init)
        : SocketPollSocketOperationBase(sock, NB_SEND), input_stream(input_stream), input_stream_obj(input_stream_obj),
          max_chunk_size(max_chunk_size), timeout_ms(timeout_ms), send_terminal_chunk(send_terminal_chunk),
          source(source) {
    init(xsink, defer_init);
}

void SocketSendHttpChunkedInputStreamPollOperation::init(ExceptionSink* xsink, bool defer_init) {
    controller_deferred_init = defer_init;
    controller_deferred_tid = defer_init ? q_gettid() : -1;
    if (!input_stream->isIoThreadSafe()) {
        xsink->raiseException("SOCKET-SEND-ERROR", "InputStream is not I/O thread safe");
        return;
    }
    if (!max_chunk_size) {
        xsink->raiseException("SOCKET-SEND-ERROR", "HTTP chunk size must be greater than 0");
        return;
    }

    stream_fd = qore_input_stream_pollable_descriptor(*input_stream, "SOCKET-SEND-ERROR", xsink);
    if (*xsink) {
        return;
    }
    is_pollable = stream_fd >= 0;

    if (defer_init) {
        return;
    }

    AutoLocker al(sock->priv->m);
    initLocked(xsink);
}

int SocketSendHttpChunkedInputStreamPollOperation::initLocked(ExceptionSink* xsink) {
    assert(sock->priv->m.trylock());
    if (initialized) {
        return 0;
    }

    if (sock->priv->checkOpen(xsink)) {
        return -1;
    }

    int rc = controller_deferred_init
        ? sock->priv->setNonBlockFromAsyncController(xsink, NB_SEND, controller_deferred_tid)
        : sock->priv->setNonBlock(xsink, NB_SEND);
    if (!rc) {
        set_non_block = true;
        initialized = true;
        return 0;
    }
    return -1;
}

QoreHashNode* SocketSendHttpChunkedInputStreamPollOperation::getPollInfo(ExceptionSink* xsink, int events,
        bool stream_wait) {
    int64 poll_timeout_ms = -1;
    if (timeout_ms >= 0) {
        int us;
        int64 now_s = q_epoch_us(us);
        int64 now_ms = now_s * 1000 + us / 1000;
        if (!wait_deadline_ms) {
            wait_deadline_ms = now_ms + timeout_ms;
        }
        poll_timeout_ms = wait_deadline_ms - now_ms;
        if (poll_timeout_ms < 0) {
            poll_timeout_ms = 0;
        }
    }

    QoreHashNode* raw_info = nullptr;
    if (stream_wait && is_pollable && stream_fd >= 0) {
        std::vector<std::pair<int, int>> extra_fds{{stream_fd, SOCK_POLLIN}};
        raw_info = getSocketPollInfoHash(xsink, 0, extra_fds);
    } else {
        raw_info = getSocketPollInfoHash(xsink, events);
    }
    if (!raw_info) {
        return nullptr;
    }
    ReferenceHolder<QoreHashNode> info(raw_info, xsink);
    if (poll_timeout_ms >= 0) {
        info->setKeyValue("poll_timeout_ms", poll_timeout_ms, xsink);
    }
    return info.release();
}

void SocketSendHttpChunkedInputStreamPollOperation::complete(ExceptionSink* xsink) {
    if (input_stream && !need_reassign) {
        input_stream->unassignThread(xsink);
        if (*xsink) {
            phase = Phase::Error;
            return;
        }
    }
    input_stream = nullptr;
    phase = Phase::Done;
    if (set_non_block) {
        sock->priv->clearNonBlock(NB_SEND);
        set_non_block = false;
    }
}

bool SocketSendHttpChunkedInputStreamPollOperation::checkTimeout(ExceptionSink* xsink) {
    if (wait_deadline_ms <= 0) {
        return false;
    }
    int us;
    int64 now_s = q_epoch_us(us);
    int64 now_ms = now_s * 1000 + us / 1000;
    if (now_ms < wait_deadline_ms) {
        return false;
    }
    xsink->raiseException("SOCKET-TIMEOUT", "socket operation timed out");
    phase = Phase::Error;
    return true;
}

void SocketSendHttpChunkedInputStreamPollOperation::clearTimeout() {
    wait_deadline_ms = 0;
}

QoreHashNode* SocketSendHttpChunkedInputStreamPollOperation::continuePoll(ExceptionSink* xsink) {
    if (need_reassign) {
        need_reassign = false;
        if (input_stream) {
            input_stream->reassignThread(xsink);
            if (*xsink) {
                phase = Phase::Error;
                return nullptr;
            }
        }
    }

    AutoLocker al(sock->priv->m);
    if (!initialized && initLocked(xsink)) {
        phase = Phase::Error;
        return nullptr;
    }
    if (sock->priv->checkOpen(xsink)) {
        phase = Phase::Error;
        return nullptr;
    }
    if (checkTimeout(xsink)) {
        return nullptr;
    }

    unsigned loop = 0;
    while (true) {
        switch (phase) {
            case Phase::ReadChunk: {
                SimpleRefHolder<BinaryNode> chunk;

                if (is_pollable) {
                    assert(stream_fd >= 0);
                    int poll_rv = qore_socket_poll_fd_now(stream_fd, POLLIN, "SOCKET-SEND-ERROR", "stream",
                        xsink);
                    if (poll_rv < 0) {
                        phase = Phase::Error;
                        return nullptr;
                    }
                    if (poll_rv == 0) {
                        return getPollInfo(xsink, SOCK_POLLIN, true);
                    }

                    chunk = new BinaryNode;
                    chunk->preallocate(max_chunk_size);
                    int64 count = input_stream->readNonBlock(
                        const_cast<void*>(chunk->getPtr()), max_chunk_size, xsink);
                    if (*xsink) {
                        phase = Phase::Error;
                        return nullptr;
                    }
                    if (count <= 0) {
                        chunk = nullptr;
                    } else {
                        chunk->setSize(count);
                    }
                } else {
                    chunk = input_stream->readHelper(max_chunk_size, xsink);
                    if (*xsink) {
                        phase = Phase::Error;
                        return nullptr;
                    }
                }

                if (!chunk || !chunk->size()) {
                    if (!send_terminal_chunk) {
                        complete(xsink);
                        return nullptr;
                    }

                    SimpleRefHolder<BinaryNode> terminal(new BinaryNode);
                    terminal->append("0\r\n\r\n", 5);
                    current_chunk = terminal.release();
                    current_data_offset = 0;
                    current_data_size = 0;
                    current_terminal_chunk = true;
                } else {
                    QoreString prefix;
                    prefix.sprintf(QLLX "\r\n", static_cast<unsigned long long>(chunk->size()));

                    SimpleRefHolder<BinaryNode> framed(new BinaryNode);
                    framed->append(prefix.c_str(), prefix.size());
                    framed->append(chunk->getPtr(), chunk->size());
                    framed->append("\r\n", 2);

                    current_data_offset = prefix.size();
                    current_data_size = chunk->size();
                    current_terminal_chunk = false;
                    current_chunk = framed.release();
                }

                clearTimeout();
                phase = Phase::SendChunk;
                continue;
            }

            case Phase::SendChunk: {
                if (!poll_state) {
                    poll_state.reset(sock->priv->socket->startSend(xsink,
                        reinterpret_cast<const char*>(current_chunk->getPtr()), current_chunk->size()));
                    if (*xsink || !poll_state) {
                        phase = Phase::Error;
                        return nullptr;
                    }
                }

                SocketSendPollState* send_state = static_cast<SocketSendPollState*>(poll_state.get());
                if (send_state->getBytesSent()) {
                    socket_data_sent = true;
                }
                size_t sent_before = send_state->getBytesSent();
                int rc = poll_state->continuePoll(xsink);
                if (*xsink) {
                    phase = Phase::Error;
                    poll_state.reset();
                    return nullptr;
                }
                if (send_state->getBytesSent() > sent_before) {
                    socket_data_sent = true;
                    clearTimeout();
                }
                if (rc) {
                    return getPollInfo(xsink, rc);
                }

                poll_state.reset();
                if (current_data_size) {
                    sock->priv->doDataEvent(QORE_EVENT_HTTP_CHUNKED_DATA_SENT, source,
                        static_cast<const char*>(current_chunk->getPtr()) + current_data_offset,
                        current_data_size);
                }
                bool terminal = current_terminal_chunk;
                current_chunk = nullptr;
                current_data_offset = 0;
                current_data_size = 0;
                current_terminal_chunk = false;

                if (terminal) {
                    complete(xsink);
                    return nullptr;
                }

                if (++loop >= max_nonblock_ops) {
                    return getPollInfo(xsink, SOCK_POLLOUT);
                }
                phase = Phase::ReadChunk;
                continue;
            }

            case Phase::Done:
            case Phase::Error:
                return nullptr;
        }
    }
}

SocketRecvOutputStreamPollOperation::SocketRecvOutputStreamPollOperation(ExceptionSink* xsink, QoreSocketObject* sock,
        OutputStream* output_stream, QoreObject* output_stream_obj, int64 size, int timeout_ms,
        bool emit_data_events)
        : SocketPollSocketOperationBase(sock, NB_RECV), output_stream(output_stream),
          output_stream_obj(output_stream_obj), size(size), timeout_ms(timeout_ms),
          emit_data_events(emit_data_events) {
    init(xsink, false);
}

SocketRecvOutputStreamPollOperation::SocketRecvOutputStreamPollOperation(ExceptionSink* xsink, QoreSocketObject* sock,
        OutputStream* output_stream, QoreObject* output_stream_obj, int64 size, int timeout_ms,
        bool emit_data_events, bool defer_init)
        : SocketPollSocketOperationBase(sock, NB_RECV), output_stream(output_stream),
          output_stream_obj(output_stream_obj), size(size), timeout_ms(timeout_ms),
          emit_data_events(emit_data_events) {
    init(xsink, defer_init);
}

void SocketRecvOutputStreamPollOperation::init(ExceptionSink* xsink, bool defer_init) {
    controller_deferred_init = defer_init;
    controller_deferred_tid = defer_init ? q_gettid() : -1;
    if (!output_stream->isIoThreadSafe()) {
        xsink->raiseException("SOCKET-RECV-ERROR", "OutputStream is not I/O thread safe");
        return;
    }

    output_fd = qore_output_stream_pollable_descriptor(*output_stream, "SOCKET-RECV-ERROR", xsink);
    if (*xsink) {
        return;
    }
    is_pollable = output_fd >= 0;

    if (defer_init) {
        return;
    }

    AutoLocker al(sock->priv->m);
    initLocked(xsink);
}

int SocketRecvOutputStreamPollOperation::initLocked(ExceptionSink* xsink) {
    assert(sock->priv->m.trylock());
    if (initialized) {
        return 0;
    }

    if (sock->priv->checkOpen(xsink)) {
        return -1;
    }

    int rc = controller_deferred_init
        ? sock->priv->setNonBlockFromAsyncController(xsink, NB_RECV, controller_deferred_tid)
        : sock->priv->setNonBlock(xsink, NB_RECV);
    if (!rc) {
        set_non_block = true;
        initialized = true;
        return 0;
    }
    return -1;
}

QoreHashNode* SocketRecvOutputStreamPollOperation::getPollInfo(ExceptionSink* xsink, int events, bool output_wait) {
    int64 poll_timeout_ms = -1;
    if (timeout_ms >= 0) {
        int us;
        int64 now_s = q_epoch_us(us);
        int64 now_ms = now_s * 1000 + us / 1000;
        if (!wait_deadline_ms) {
            wait_deadline_ms = now_ms + timeout_ms;
        }
        poll_timeout_ms = wait_deadline_ms - now_ms;
        if (poll_timeout_ms < 0) {
            poll_timeout_ms = 0;
        }
    }

    QoreHashNode* raw_info = nullptr;
    if (output_wait && is_pollable && output_fd >= 0) {
        std::vector<std::pair<int, int>> extra_fds{{output_fd, SOCK_POLLOUT}};
        raw_info = getSocketPollInfoHash(xsink, 0, extra_fds);
    } else {
        raw_info = getSocketPollInfoHash(xsink, events);
    }
    if (!raw_info) {
        return nullptr;
    }
    ReferenceHolder<QoreHashNode> info(raw_info, xsink);
    if (poll_timeout_ms >= 0) {
        info->setKeyValue("poll_timeout_ms", poll_timeout_ms, xsink);
    }
    return info.release();
}

int64 SocketRecvOutputStreamPollOperation::getNextChunkSize() const {
    if (size < 0) {
        return DEFAULT_SOCKET_BUFSIZE;
    }
    return QORE_MIN(size - bytes_received, (int64)DEFAULT_SOCKET_BUFSIZE);
}

void SocketRecvOutputStreamPollOperation::complete(ExceptionSink* xsink) {
    if (output_stream && !need_reassign) {
        output_stream->unassignThread(xsink);
        if (*xsink) {
            phase = Phase::Error;
            return;
        }
    }
    output_stream = nullptr;
    phase = Phase::Done;
    if (set_non_block) {
        sock->priv->clearNonBlock(NB_RECV);
        set_non_block = false;
    }
}

bool SocketRecvOutputStreamPollOperation::checkTimeout(ExceptionSink* xsink) {
    if (wait_deadline_ms <= 0) {
        return false;
    }
    int us;
    int64 now_s = q_epoch_us(us);
    int64 now_ms = now_s * 1000 + us / 1000;
    if (now_ms < wait_deadline_ms) {
        return false;
    }
    xsink->raiseException("SOCKET-TIMEOUT", "socket operation timed out");
    phase = Phase::Error;
    return true;
}

void SocketRecvOutputStreamPollOperation::clearTimeout() {
    wait_deadline_ms = 0;
}

QoreHashNode* SocketRecvOutputStreamPollOperation::continuePoll(ExceptionSink* xsink) {
    if (need_reassign) {
        need_reassign = false;
        if (output_stream) {
            output_stream->reassignThread(xsink);
            if (*xsink) {
                phase = Phase::Error;
                return nullptr;
            }
        }
    }

    AutoLocker al(sock->priv->m);
    if (!initialized && initLocked(xsink)) {
        phase = Phase::Error;
        return nullptr;
    }
    if (phase == Phase::RecvChunk && size < 0 && bytes_received > 0 && !sock->priv->socket->isOpen()) {
        complete(xsink);
        return nullptr;
    }
    if (sock->priv->checkOpen(xsink)) {
        phase = Phase::Error;
        return nullptr;
    }
    if (checkTimeout(xsink)) {
        return nullptr;
    }

    unsigned loop = 0;
    while (true) {
        switch (phase) {
            case Phase::RecvChunk: {
                if (size >= 0 && bytes_received >= size) {
                    complete(xsink);
                    return nullptr;
                }

                int64 chunk_size = getNextChunkSize();
                if (chunk_size <= 0) {
                    complete(xsink);
                    return nullptr;
                }

                if (!poll_state) {
                    poll_state.reset(sock->priv->socket->startRecvSome(xsink, static_cast<size_t>(chunk_size)));
                    if (*xsink || !poll_state) {
                        phase = Phase::Error;
                        return nullptr;
                    }
                }

                int rc = poll_state->continuePoll(xsink);
                if (*xsink) {
                    phase = Phase::Error;
                    poll_state.reset();
                    return nullptr;
                }
                if (rc) {
                    return getPollInfo(xsink, rc);
                }

                SimpleRefHolder<BinaryNode> chunk(poll_state->takeOutput().get<BinaryNode>());
                poll_state.reset();
                if (!chunk || !chunk->size()) {
                    if (size >= 0) {
                        xsink->raiseException("SOCKET-RECV-ERROR", "Unexpected end of stream");
                        phase = Phase::Error;
                        return nullptr;
                    }
                    complete(xsink);
                    return nullptr;
                }

                bytes_received += chunk->size();
                if (emit_data_events) {
                    qore_socket_private::get(*sock->priv->socket)->do_data_event(
                        QORE_EVENT_SOCKET_DATA_READ, QORE_SOURCE_SOCKET, **chunk);
                }
                current_chunk = chunk.release();
                write_offset = 0;
                clearTimeout();
                phase = Phase::WriteChunk;
                continue;
            }

            case Phase::WriteChunk: {
                while (write_offset < current_chunk->size()) {
                    if (is_pollable) {
                        assert(output_fd >= 0);
                        int poll_rv = qore_socket_poll_fd_now(output_fd, POLLOUT, "SOCKET-RECV-ERROR",
                            "output stream", xsink);
                        if (poll_rv < 0) {
                            phase = Phase::Error;
                            return nullptr;
                        }
                        if (poll_rv == 0) {
                            return getPollInfo(xsink, SOCK_POLLIN, true);
                        }
                    }

                    const char* ptr = reinterpret_cast<const char*>(current_chunk->getPtr()) + write_offset;
                    size_t remaining = current_chunk->size() - write_offset;
                    int64 written = output_stream->writeNonBlock(ptr, remaining, xsink);
                    if (*xsink || written < 0) {
                        phase = Phase::Error;
                        return nullptr;
                    }
                    if (!written) {
                        return getPollInfo(xsink, SOCK_POLLIN, true);
                    }

                    write_offset += static_cast<size_t>(written);
                    clearTimeout();
                    if (++loop >= max_nonblock_ops && write_offset < current_chunk->size()) {
                        return getPollInfo(xsink, SOCK_POLLIN, true);
                    }
                }

                current_chunk = nullptr;
                write_offset = 0;
                if (++loop >= max_nonblock_ops && (size < 0 || bytes_received < size)) {
                    return getPollInfo(xsink, SOCK_POLLIN);
                }
                phase = Phase::RecvChunk;
                continue;
            }

            case Phase::Done:
            case Phase::Error:
                return nullptr;
        }
    }
}

SocketWriteOutputStreamPollOperation::SocketWriteOutputStreamPollOperation(ExceptionSink* xsink,
        QoreSocketObject* sock, OutputStream* output_stream, QoreObject* output_stream_obj, BinaryNode* data,
        int timeout_ms)
        : sock(sock), output_stream(output_stream), output_stream_obj(output_stream_obj), data(data),
          timeout_ms(timeout_ms) {
    if (!output_stream->isIoThreadSafe()) {
        xsink->raiseException("SOCKET-WRITE-ERROR", "OutputStream is not I/O thread safe");
        return;
    }

    output_fd = qore_output_stream_pollable_descriptor(output_stream, "SOCKET-WRITE-ERROR", xsink);
    if (*xsink) {
        return;
    }
    is_pollable = output_fd >= 0;
}

QoreHashNode* SocketWriteOutputStreamPollOperation::getPollInfo(ExceptionSink* xsink) {
    int64 poll_timeout_ms = -1;
    if (timeout_ms >= 0) {
        int us;
        int64 now_s = q_epoch_us(us);
        int64 now_ms = now_s * 1000 + us / 1000;
        if (!wait_deadline_ms) {
            wait_deadline_ms = now_ms + timeout_ms;
        }
        poll_timeout_ms = wait_deadline_ms - now_ms;
        if (poll_timeout_ms < 0) {
            poll_timeout_ms = 0;
        }
    }

    QoreHashNode* raw_info = nullptr;
    if (is_pollable && output_fd >= 0) {
        std::vector<std::pair<int, int>> extra_fds{{output_fd, SOCK_POLLOUT}};
        raw_info = getSocketPollInfoHash(xsink, 0, extra_fds);
    } else {
        raw_info = getSocketPollInfoHash(xsink, SOCK_POLLIN);
    }
    if (!raw_info) {
        return nullptr;
    }
    ReferenceHolder<QoreHashNode> info(raw_info, xsink);
    if (poll_timeout_ms >= 0) {
        info->setKeyValue("poll_timeout_ms", poll_timeout_ms, xsink);
    }
    return info.release();
}

void SocketWriteOutputStreamPollOperation::complete(ExceptionSink* xsink) {
    if (output_stream && !need_reassign) {
        output_stream->unassignThread(xsink);
        if (*xsink) {
            phase = Phase::Error;
            return;
        }
    }
    output_stream = nullptr;
    data = nullptr;
    phase = Phase::Done;
}

bool SocketWriteOutputStreamPollOperation::checkTimeout(ExceptionSink* xsink) {
    if (wait_deadline_ms <= 0) {
        return false;
    }
    int us;
    int64 now_s = q_epoch_us(us);
    int64 now_ms = now_s * 1000 + us / 1000;
    if (now_ms < wait_deadline_ms) {
        return false;
    }
    xsink->raiseException("SOCKET-TIMEOUT", "socket operation timed out");
    phase = Phase::Error;
    return true;
}

void SocketWriteOutputStreamPollOperation::clearTimeout() {
    wait_deadline_ms = 0;
}

QoreHashNode* SocketWriteOutputStreamPollOperation::continuePoll(ExceptionSink* xsink) {
    if (need_reassign) {
        need_reassign = false;
        if (output_stream) {
            output_stream->reassignThread(xsink);
            if (*xsink) {
                phase = Phase::Error;
                return nullptr;
            }
        }
    }

    if (checkTimeout(xsink)) {
        return nullptr;
    }

    unsigned loop = 0;
    while (phase == Phase::Write && write_offset < data->size()) {
        if (is_pollable) {
            assert(output_fd >= 0);
            int poll_rv = qore_socket_poll_fd_now(output_fd, POLLOUT, "SOCKET-WRITE-ERROR", "output stream",
                xsink);
            if (poll_rv < 0) {
                phase = Phase::Error;
                return nullptr;
            }
            if (!poll_rv) {
                return getPollInfo(xsink);
            }
        }

        const char* ptr = reinterpret_cast<const char*>(data->getPtr()) + write_offset;
        size_t remaining = data->size() - write_offset;
        int64 written = output_stream->writeNonBlock(ptr, remaining, xsink);
        if (*xsink || written < 0) {
            phase = Phase::Error;
            return nullptr;
        }
        if (!written) {
            return getPollInfo(xsink);
        }

        write_offset += static_cast<size_t>(written);
        clearTimeout();
        if (++loop >= max_nonblock_ops && write_offset < data->size()) {
            return getPollInfo(xsink);
        }
    }

    complete(xsink);
    return nullptr;
}

QoreHashNode* SocketRecvPollOperationBase::continuePoll(ExceptionSink* xsink) {
    AutoLocker al(sock->priv->m);

    if (!initialized) {
        if (initPollState(xsink)) {
            return nullptr;
        }
    } else if (sock->priv->checkOpen(xsink)) {
        return nullptr;
    }

    if (!poll_state) {
        return nullptr;
    }

    // see if we are able to continue
    int rc = poll_state->continuePoll(xsink);
    //printd(5, "SocketRecvPollOperation::continuePoll() state: %s rc: %d (exp: %d)\n", getStateImpl(), rc,
    //    (int)*xsink);
    if (!rc) {
        // get output data
        SimpleRefHolder<BinaryNode> d(poll_state->takeOutput().get<BinaryNode>());
        bool ok = true;
        if (to_string) {
            size_t len = d->size();
            char* buf = reinterpret_cast<char*>(d->giveBuffer());
            char* nbuf = reinterpret_cast<char*>(q_realloc(buf, len + 1));
            if (!nbuf) {
                xsink->outOfMemory();
                ok = false;
            } else {
                nbuf[len] = '\0';
                data = new QoreStringNode(nbuf, len, len + 1, sock->getEncoding());
            }
        } else {
            data = d.release();
        }
        if (ok) {
            received = true;
        }
    }
    if (*xsink || !rc) {
        // release the AbstractPollState value
        poll_state.reset();
        sock->priv->clearNonBlock(NB_RECV);
        set_non_block = false;
        return nullptr;
    }
    return getSocketPollInfoHash(xsink, rc);
}

int SocketRecvPollOperationBase::initPollState(ExceptionSink* xsink) {
    xsink->raiseException("SOCKET-RECV-ERROR", "missing receive poll state initializer");
    return -1;
}

int SocketRecvPollOperationBase::initIntern(ExceptionSink* xsink) {
    assert(sock->priv->m.trylock());

    // throw an exception and exit if the object is no longer open or valid
    if (sock->priv->checkOpen(xsink)) {
        return -1;
    }

    int rc = controller_deferred_init
        ? sock->priv->setNonBlockFromAsyncController(xsink, NB_RECV, controller_deferred_tid)
        : sock->priv->setNonBlock(xsink, NB_RECV);
    if (rc) {
        return -1;
    }

    set_non_block = true;
    return 0;
}

SocketRecvDataPollOperation::SocketRecvDataPollOperation(ExceptionSink* xsink, QoreSocketObject* sock, bool to_string)
        : SocketRecvPollOperationBase(sock, to_string) {
    init(xsink, false);
}

SocketRecvDataPollOperation::SocketRecvDataPollOperation(ExceptionSink* xsink, QoreSocketObject* sock,
        bool to_string, bool defer_init) : SocketRecvPollOperationBase(sock, to_string) {
    init(xsink, defer_init);
}

void SocketRecvDataPollOperation::init(ExceptionSink* xsink, bool defer_init) {
    controller_deferred_init = defer_init;
    controller_deferred_tid = defer_init ? q_gettid() : -1;
    if (defer_init) {
        return;
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*sock);
    AutoLocker al(priv->m);
    initPollState(xsink);
}

int SocketRecvDataPollOperation::initPollState(ExceptionSink* xsink) {
    assert(sock->priv->m.trylock());
    if (initialized) {
        return 0;
    }
    if (initIntern(xsink)) {
        return -1;
    }

    poll_state.reset(sock->priv->socket->startRecvPacket(xsink));
    if (*xsink) {
        sock->priv->clearNonBlock(NB_RECV);
        set_non_block = false;
        return -1;
    }
    initialized = true;
    return poll_state ? 0 : -1;
}

bool SocketRecvDataPollOperation::abortNeedsClose() const {
    if (poll_state) {
        assert(dynamic_cast<SocketRecvPacketPollState*>(poll_state.get()));
        return reinterpret_cast<SocketRecvPacketPollState*>(poll_state.get())->getBytesReceived() ? true : false;
    }
    return true;
}

SocketRecvPollOperation::SocketRecvPollOperation(ExceptionSink* xsink, ssize_t size, QoreSocketObject* sock, bool to_string)
        : SocketRecvPollOperationBase(sock, to_string), size(size) {
    init(xsink, false);
}

SocketRecvPollOperation::SocketRecvPollOperation(ExceptionSink* xsink, ssize_t size, QoreSocketObject* sock,
        bool to_string, bool defer_init) : SocketRecvPollOperationBase(sock, to_string), size(size) {
    init(xsink, defer_init);
}

void SocketRecvPollOperation::init(ExceptionSink* xsink, bool defer_init) {
    controller_deferred_init = defer_init;
    controller_deferred_tid = defer_init ? q_gettid() : -1;
    if (defer_init) {
        return;
    }

    AutoLocker al(sock->priv->m);
    initPollState(xsink);
}

int SocketRecvPollOperation::initPollState(ExceptionSink* xsink) {
    assert(sock->priv->m.trylock());
    if (initialized) {
        return 0;
    }
    if (initIntern(xsink)) {
        return -1;
    }

    poll_state.reset(sock->priv->socket->startRecv(xsink, size));
    if (*xsink) {
        sock->priv->clearNonBlock(NB_RECV);
        set_non_block = false;
        return -1;
    }
    initialized = true;
    return poll_state ? 0 : -1;
}

bool SocketRecvPollOperation::abortNeedsClose() const {
    if (poll_state) {
        assert(dynamic_cast<SocketRecvPollState*>(poll_state.get()));
        return reinterpret_cast<SocketRecvPollState*>(poll_state.get())->getBytesReceived() ? true : false;
    }
    return true;
}

SocketRecvSomePollOperation::SocketRecvSomePollOperation(ExceptionSink* xsink, ssize_t size, QoreSocketObject* sock,
        bool to_string) : SocketRecvPollOperationBase(sock, to_string), size(size > 0 ? size : 0) {
    init(xsink, false);
}

SocketRecvSomePollOperation::SocketRecvSomePollOperation(ExceptionSink* xsink, ssize_t size, QoreSocketObject* sock,
        bool to_string, bool defer_init) : SocketRecvPollOperationBase(sock, to_string), size(size > 0 ? size : 0) {
    init(xsink, defer_init);
}

void SocketRecvSomePollOperation::init(ExceptionSink* xsink, bool defer_init) {
    controller_deferred_init = defer_init;
    controller_deferred_tid = defer_init ? q_gettid() : -1;
    if (defer_init) {
        return;
    }

    AutoLocker al(sock->priv->m);
    initPollState(xsink);
}

int SocketRecvSomePollOperation::initPollState(ExceptionSink* xsink) {
    assert(sock->priv->m.trylock());
    if (initialized) {
        return 0;
    }
    if (initIntern(xsink)) {
        return -1;
    }

    poll_state.reset(sock->priv->socket->startRecvSome(xsink, this->size));
    if (*xsink) {
        sock->priv->clearNonBlock(NB_RECV);
        set_non_block = false;
        return -1;
    }
    initialized = true;
    return poll_state ? 0 : -1;
}

bool SocketRecvSomePollOperation::abortNeedsClose() const {
    if (poll_state) {
        assert(dynamic_cast<SocketRecvSomePollState*>(poll_state.get()));
        return reinterpret_cast<SocketRecvSomePollState*>(poll_state.get())->getBytesReceived() ? true : false;
    }
    return true;
}

SocketRecvUntilBytesPollOperation::SocketRecvUntilBytesPollOperation(ExceptionSink* xsink, const QoreStringNode* pattern,
        QoreSocketObject* sock, bool to_string) : SocketRecvPollOperationBase(sock, to_string),
        pattern(pattern->stringRefSelf()) {
    init(xsink, false);
}

SocketRecvUntilBytesPollOperation::SocketRecvUntilBytesPollOperation(ExceptionSink* xsink,
        const QoreStringNode* pattern, QoreSocketObject* sock, bool to_string, bool defer_init)
        : SocketRecvPollOperationBase(sock, to_string), pattern(pattern->stringRefSelf()) {
    init(xsink, defer_init);
}

void SocketRecvUntilBytesPollOperation::init(ExceptionSink* xsink, bool defer_init) {
    controller_deferred_init = defer_init;
    controller_deferred_tid = defer_init ? q_gettid() : -1;
    if (defer_init) {
        return;
    }

    AutoLocker al(sock->priv->m);
    initPollState(xsink);
}

int SocketRecvUntilBytesPollOperation::initPollState(ExceptionSink* xsink) {
    assert(sock->priv->m.trylock());
    if (initialized) {
        return 0;
    }
    if (initIntern(xsink)) {
        return -1;
    }

    poll_state.reset(sock->priv->socket->startRecvUntilBytes(xsink, pattern->c_str(), pattern->size()));
    if (*xsink) {
        sock->priv->clearNonBlock(NB_RECV);
        set_non_block = false;
        return -1;
    }
    initialized = true;
    return poll_state ? 0 : -1;
}

bool SocketRecvUntilBytesPollOperation::abortNeedsClose() const {
    if (poll_state) {
        assert(dynamic_cast<SocketRecvUntilBytesPollState*>(poll_state.get()));
        return reinterpret_cast<SocketRecvUntilBytesPollState*>(poll_state.get())->getBytesReceived() ? true : false;
    }
    return true;
}

SocketReadHttpHeaderPollOperation::SocketReadHttpHeaderPollOperation(ExceptionSink* xsink, QoreSocketObject* sock)
        : SocketRecvPollOperationBase(sock, true), out(xsink) {
    init(xsink, false);
}

SocketReadHttpHeaderPollOperation::SocketReadHttpHeaderPollOperation(ExceptionSink* xsink, QoreSocketObject* sock,
        bool defer_init) : SocketRecvPollOperationBase(sock, true), out(xsink) {
    init(xsink, defer_init);
}

void SocketReadHttpHeaderPollOperation::init(ExceptionSink* xsink, bool defer_init) {
    controller_deferred_init = defer_init;
    controller_deferred_tid = defer_init ? q_gettid() : -1;
    if (defer_init) {
        return;
    }

    AutoLocker al(sock->priv->m);
    initPollState(xsink);
}

int SocketReadHttpHeaderPollOperation::initPollState(ExceptionSink* xsink) {
    assert(sock->priv->m.trylock());
    if (initialized) {
        return 0;
    }
    if (initIntern(xsink)) {
        return -1;
    }

    poll_state.reset(sock->priv->socket->startRecvUntilBytes(xsink, "\r\n\r\n", 4));
    if (*xsink) {
        sock->priv->clearNonBlock(NB_RECV);
        set_non_block = false;
        return -1;
    }
    initialized = true;
    return poll_state ? 0 : -1;
}

QoreHashNode* SocketReadHttpHeaderPollOperation::continuePoll(ExceptionSink* xsink) {
    QoreHashNode* rv = SocketRecvPollOperationBase::continuePoll(xsink);
    if (rv || !data) {
        return rv;
    }

    ReferenceHolder<QoreHashNode> info(new QoreHashNode(autoTypeInfo), xsink);
    assert(data->getType() == NT_STRING);
    SimpleRefHolder<QoreStringNode> hdrstr(reinterpret_cast<QoreStringNode*>(data.release()));
    ReferenceHolder<QoreHashNode> hdr(sock->priv->socket->priv->processHttpHeaderString(xsink, hdrstr, *info,
        QORE_SOURCE_SOCKET), xsink);
    if (*xsink) {
        return nullptr;
    }
    // Store result in member variable for getOutput() - don't return it from continuePoll()
    out = new QoreHashNode(autoTypeInfo);
    out->setKeyValue("hdr", hdr.release(), xsink);
    out->setKeyValue("info", info.release(), xsink);
    // Return nullptr to indicate operation is complete; result is available via getOutput()
    return nullptr;
}

QoreValue SocketReadHttpHeaderPollOperation::getOutput() const {
    AutoLocker al(sock->priv->m);
    return out.release();
}

bool SocketReadHttpHeaderPollOperation::abortNeedsClose() const {
    if (poll_state) {
        assert(dynamic_cast<SocketRecvUntilBytesPollState*>(poll_state.get()));
        return reinterpret_cast<SocketRecvUntilBytesPollState*>(poll_state.get())->getBytesReceived() ? true : false;
    }
    return true;
}

SocketReadServerSentEventPollOperation::SocketReadServerSentEventPollOperation(ExceptionSink* xsink,
        QoreSocketObject* sock) : SocketRecvPollOperationBase(sock, false), event_data(QCS_UTF8),
        compressed_data(QCS_DEFAULT), out(xsink) {
    init(xsink, false);
}

SocketReadServerSentEventPollOperation::SocketReadServerSentEventPollOperation(ExceptionSink* xsink,
        QoreSocketObject* sock, bool defer_init) : SocketRecvPollOperationBase(sock, false), event_data(QCS_UTF8),
        compressed_data(QCS_DEFAULT), out(xsink) {
    init(xsink, defer_init);
}

SocketReadServerSentEventPollOperation::SocketReadServerSentEventPollOperation(ExceptionSink* xsink,
        QoreSocketObject* sock, const QoreStringNode* content_encoding, bool defer_init)
        : SocketRecvPollOperationBase(sock, false), event_data(QCS_UTF8), compressed_data(QCS_DEFAULT),
        out(xsink) {
    initTransform(xsink, content_encoding);
    if (*xsink) {
        return;
    }
    init(xsink, defer_init);
}

void SocketReadServerSentEventPollOperation::initTransform(ExceptionSink* xsink,
        const QoreStringNode* content_encoding) {
    if (!content_encoding) {
        return;
    }

    transform = CompressionTransforms::getDecompressor(content_encoding, xsink);
    if (*xsink) {
        return;
    }

    transform_buf_size = transform->outputBufferSize();
    if (!transform_buf_size) {
        return;
    }
    transform_buf.reset(new (std::nothrow) char[transform_buf_size]);
    if (!transform_buf) {
        xsink->outOfMemory();
    }
}

void SocketReadServerSentEventPollOperation::init(ExceptionSink* xsink, bool defer_init) {
    controller_deferred_init = defer_init;
    controller_deferred_tid = defer_init ? q_gettid() : -1;
    if (defer_init) {
        return;
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*sock);
    AutoLocker al(priv->m);
    initPollState(xsink);
}

int SocketReadServerSentEventPollOperation::initPollState(ExceptionSink* xsink) {
    assert(my_socket_priv::getPriv(*sock)->m.trylock());
    if (initialized) {
        return 0;
    }
    if (initIntern(xsink)) {
        return -1;
    }

    initialized = true;
    return 0;
}

bool SocketReadServerSentEventPollOperation::processSseChar(ExceptionSink* xsink, qore_socket_private* sp,
        char c) {
    if (!qore_socket_exec_process_sse_char(sp, event_data, eol_count, c)) {
        return false;
    }

    if (set_non_block) {
        my_socket_priv::getPriv(*sock)->clearNonBlock(NB_RECV);
        set_non_block = false;
    }
    ReferenceHolder<QoreHashNode> parsed(QoreSocket::parseServerSentEvent(xsink, event_data), xsink);
    if (*xsink) {
        return true;
    }
    out = parsed.release();
    received = true;
    return true;
}

QoreHashNode* SocketReadServerSentEventPollOperation::continuePoll(ExceptionSink* xsink) {
    my_socket_priv* priv = my_socket_priv::getPriv(*sock);
    AutoLocker al(priv->m);

    if (received) {
        return nullptr;
    }

    if (!initialized) {
        if (initPollState(xsink)) {
            return nullptr;
        }
    } else if (priv->checkOpen(xsink)) {
        return nullptr;
    }

    qore_socket_private* sp = qore_socket_private::get(*priv->socket);
    while (true) {
        if (transform && transform_pos < transform_len) {
            char c = transform_buf[transform_pos++];
            if (processSseChar(xsink, sp, c)) {
                return nullptr;
            }
            continue;
        }

        if (!poll_state) {
            poll_state.reset(transform
                ? priv->socket->startRecvSome(xsink, DEFAULT_SOCKET_BUFSIZE)
                : priv->socket->startRecv(xsink, 1));
            if (*xsink || !poll_state) {
                if (set_non_block) {
                    priv->clearNonBlock(NB_RECV);
                    set_non_block = false;
                }
                return nullptr;
            }
        }

        int rc = poll_state->continuePoll(xsink);
        if (*xsink) {
            poll_state.reset();
            if (set_non_block) {
                priv->clearNonBlock(NB_RECV);
                set_non_block = false;
            }
            return nullptr;
        }
        if (rc) {
            return getSocketPollInfoHash(xsink, rc);
        }

        SimpleRefHolder<BinaryNode> data(poll_state->takeOutput().get<BinaryNode>());
        poll_state.reset();
        if (!data || !data->size()) {
            if (set_non_block) {
                priv->clearNonBlock(NB_RECV);
                set_non_block = false;
            }
            se_closed("Socket", "readServerSentEvent", xsink);
            return nullptr;
        }

        bytes_consumed = true;
        if (transform) {
            compressed_data.concat(static_cast<const char*>(data->getPtr()), data->size());
            transform_pos = 0;
            std::pair<int64, int64> result = transform->apply(compressed_data.c_str(), compressed_data.size(),
                transform_buf.get(), transform_buf_size, xsink);
            if (*xsink) {
                if (set_non_block) {
                    priv->clearNonBlock(NB_RECV);
                    set_non_block = false;
                }
                return nullptr;
            }
            if (result.first) {
                compressed_data.removeBytes(result.first);
            }
            transform_len = static_cast<size_t>(result.second);
            if (!transform_len) {
                continue;
            }
            continue;
        }

        char c = *static_cast<const char*>(data->getPtr());
        if (processSseChar(xsink, sp, c)) {
            return nullptr;
        }
    }
}

QoreValue SocketReadServerSentEventPollOperation::getOutput() const {
    my_socket_priv* priv = my_socket_priv::getPriv(*sock);
    AutoLocker al(priv->m);
    return out.release();
}

void SocketReadServerSentEventPollOperation::abort(ExceptionSink* xsink) {
    out = nullptr;
    SocketRecvPollOperationBase::abort(xsink);
}

const char* SocketReadServerSentEventPollOperation::getStateImpl() const {
    return received ? "received" : "reading-sse";
}

bool SocketReadServerSentEventPollOperation::abortNeedsClose() const {
    if (bytes_consumed) {
        return true;
    }
    if (poll_state) {
        if (transform) {
            assert(dynamic_cast<SocketRecvSomePollState*>(poll_state.get()));
            return reinterpret_cast<SocketRecvSomePollState*>(poll_state.get())->getBytesReceived() ? true : false;
        } else {
            assert(dynamic_cast<SocketRecvPollState*>(poll_state.get()));
            return reinterpret_cast<SocketRecvPollState*>(poll_state.get())->getBytesReceived() ? true : false;
        }
    }
    return true;
}

SocketReadHttpChunkedBodyPollOperation::SocketReadHttpChunkedBodyPollOperation(ExceptionSink* xsink,
        QoreSocketObject* sock, bool binary_body, bool read_once, int source)
        : SocketRecvPollOperationBase(sock, false), out(xsink),
          body_bin(binary_body ? new BinaryNode : nullptr),
          body_str(binary_body ? nullptr : new QoreStringNode(sock->getEncoding())),
          trailers(sock->getEncoding()), binary_body(binary_body), read_once(read_once), source(source),
          authorized_tid(q_gettid()) {
    assert(!read_once || binary_body);
}

void SocketReadHttpChunkedBodyPollOperation::cleanup(ExceptionSink* xsink) {
    out = nullptr;
    body_bin.discard();
    body_str.discard();
    data.discard();
    if (set_non_block) {
        my_socket_priv* priv = my_socket_priv::getPriv(*sock);
        AutoLocker al(priv->m);
        priv->clearNonBlock(NB_RECV);
        set_non_block = false;
    }
    poll_state.reset();
    if (sock) {
        sock->deref(xsink);
        sock = nullptr;
    }
}

void SocketReadHttpChunkedBodyPollOperation::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        cleanup(xsink);
        delete this;
    }
}

void SocketReadHttpChunkedBodyPollOperation::startCurrentOp(ExceptionSink* xsink) {
    assert(!poll_state);

    my_socket_priv* priv = my_socket_priv::getPriv(*sock);
    AutoLocker al(priv->m);
    if (priv->checkOpen(xsink)) {
        return;
    }
    if (priv->setNonBlockFromAsyncController(xsink, NB_RECV, authorized_tid)) {
        return;
    }
    set_non_block = true;
    switch (chunked_body_state) {
        case ChunkedBodyState::RECV_CHUNK_SIZE:
        case ChunkedBodyState::RECV_TRAILER: {
            current_read_to_string = true;
            poll_state.reset(priv->socket->startRecvUntilBytes(xsink, "\r\n", 2));
            break;
        }
        case ChunkedBodyState::RECV_CHUNK_DATA:
            current_read_to_string = false;
            poll_state.reset(priv->socket->startRecv(xsink, static_cast<size_t>(current_chunk_size + 2)));
            break;
        case ChunkedBodyState::DONE:
            priv->clearNonBlock(NB_RECV);
            set_non_block = false;
            return;
    }

    if (*xsink || !poll_state) {
        priv->clearNonBlock(NB_RECV);
        set_non_block = false;
    }
}

int SocketReadHttpChunkedBodyPollOperation::setOutputBody(ExceptionSink* xsink) {
    out = new QoreHashNode(autoTypeInfo);
    if (binary_body) {
        out->setKeyValue("body", body_bin.release(), xsink);
    } else {
        out->setKeyValue("body", body_str.release(), xsink);
    }
    return *xsink ? -1 : 0;
}

int SocketReadHttpChunkedBodyPollOperation::handleChunkSize(ExceptionSink* xsink) {
    ValueHolder line_val(data ? data->refSelf() : QoreValue(), xsink);
    data.discard();
    if (*xsink) {
        return -1;
    }
    if (line_val->getType() != NT_STRING) {
        xsink->raiseException("READ-HTTP-CHUNK-ERROR",
            "expected string output from async chunk-size read operation, got '%s'", line_val->getFullTypeName());
        return -1;
    }

    const QoreStringNode* line = line_val->get<const QoreStringNode>();
    const char* line_str = line->c_str();
    size_t line_len = line->size();
    if (line_len >= 2) {
        line_len -= 2;
    }

    int64_t chunk_size = 0;
    if (qore_socket_exec_parse_http_chunk_size(line_str, line_len, chunk_size, xsink)) {
        return -1;
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*sock);
    priv->doChunkedReadEvent(QORE_EVENT_HTTP_CHUNK_SIZE, static_cast<size_t>(chunk_size), line_len, source);
    bytes_consumed = true;

    if (!chunk_size) {
        if (setOutputBody(xsink)) {
            return -1;
        }
        chunked_body_state = ChunkedBodyState::RECV_TRAILER;
        return 0;
    }

    current_chunk_size = chunk_size;
    chunked_body_state = ChunkedBodyState::RECV_CHUNK_DATA;
    return 0;
}

int SocketReadHttpChunkedBodyPollOperation::handleChunkData(ExceptionSink* xsink) {
    ValueHolder data_val(data ? data->refSelf() : QoreValue(), xsink);
    data.discard();
    if (*xsink) {
        return -1;
    }
    if (data_val->getType() != NT_BINARY) {
        xsink->raiseException("READ-HTTP-CHUNK-ERROR",
            "expected binary output from async chunk data read operation, got '%s'", data_val->getFullTypeName());
        return -1;
    }

    const BinaryNode* bin = data_val->get<const BinaryNode>();
    size_t chunk_size = static_cast<size_t>(current_chunk_size);
    if (bin->size() < chunk_size + 2) {
        xsink->raiseException("READ-HTTP-CHUNK-ERROR", "read " QSD " bytes for HTTP chunk size " QSD,
            bin->size(), chunk_size);
        return -1;
    }

    my_socket_priv* priv = my_socket_priv::getPriv(*sock);
    priv->doDataEvent(QORE_EVENT_HTTP_CHUNKED_DATA_READ, source, bin->getPtr(), chunk_size);
    if (binary_body) {
        body_bin->append(bin->getPtr(), chunk_size);
    } else {
        body_str->concat(static_cast<const char*>(bin->getPtr()), chunk_size);
    }
    bytes_consumed = true;

    int64 body_size = binary_body ? static_cast<int64>(body_bin->size()) : static_cast<int64>(body_str->size());
    int64 max_chunked_body_size = priv->getMaxChunkedBodySize();
    if (max_chunked_body_size > 0 && body_size > max_chunked_body_size) {
        xsink->raiseException("HTTP-BODY-TOO-LARGE", "chunked body size " QLLD " exceeds maximum " QLLD,
            body_size, max_chunked_body_size);
        return -1;
    }

    priv->doChunkedReadEvent(QORE_EVENT_HTTP_CHUNKED_DATA_RECEIVED, chunk_size, chunk_size + 2, source);
    if (read_once) {
        if (setOutputBody(xsink)) {
            return -1;
        }
        chunked_body_state = ChunkedBodyState::DONE;
        received = true;
        return 0;
    }

    current_chunk_size = 0;
    chunked_body_state = ChunkedBodyState::RECV_CHUNK_SIZE;
    return 0;
}

int SocketReadHttpChunkedBodyPollOperation::handleTrailer(ExceptionSink* xsink) {
    ValueHolder line_val(data ? data->refSelf() : QoreValue(), xsink);
    data.discard();
    if (*xsink) {
        return -1;
    }
    if (line_val->getType() != NT_STRING) {
        xsink->raiseException("READ-HTTP-CHUNK-ERROR",
            "expected string output from async chunk trailer read operation, got '%s'",
            line_val->getFullTypeName());
        return -1;
    }

    const QoreStringNode* line = line_val->get<const QoreStringNode>();
    bytes_consumed = true;
    if (line->size() == 2 && !strcmp(line->c_str(), "\r\n")) {
        if (!trailers.empty()) {
            my_socket_priv* priv = my_socket_priv::getPriv(*sock);
            priv->convertHeaderToHash(**out, trailers);
            if (*xsink) {
                return -1;
            }
            priv->doReadHttpHeaderEvent(QORE_EVENT_HTTP_FOOTERS_RECEIVED, **out, source);
        }
        chunked_body_state = ChunkedBodyState::DONE;
        received = true;
        return 0;
    }

    trailers.concat(line->c_str(), line->size());
    return 0;
}

QoreHashNode* SocketReadHttpChunkedBodyPollOperation::continuePoll(ExceptionSink* xsink) {
    if (received) {
        return nullptr;
    }

    if (!http_expect_cleared) {
        my_socket_priv::getPriv(*sock)->clearHttpExpectChunkedBody();
        http_expect_cleared = true;
    }

    unsigned cancel_check = 0;
    while (!received) {
        if (!(cancel_check++ % 100) && qore_check_cancel(xsink, "socket chunked body read")) {
            chunked_body_state = ChunkedBodyState::DONE;
            return nullptr;
        }

        if (!poll_state) {
            startCurrentOp(xsink);
            if (*xsink) {
                chunked_body_state = ChunkedBodyState::DONE;
                return nullptr;
            }
            if (!poll_state) {
                chunked_body_state = ChunkedBodyState::DONE;
                received = true;
                return nullptr;
            }
        }

        int poll_rc = 0;
        {
            my_socket_priv* priv = my_socket_priv::getPriv(*sock);
            AutoLocker al(priv->m);

            if (priv->checkOpen(xsink)) {
                chunked_body_state = ChunkedBodyState::DONE;
                return nullptr;
            }

            poll_rc = poll_state->continuePoll(xsink);
            if (*xsink) {
                poll_state.reset();
                priv->clearNonBlock(NB_RECV);
                set_non_block = false;
                chunked_body_state = ChunkedBodyState::DONE;
                return nullptr;
            }
            if (!poll_rc) {
                SimpleRefHolder<BinaryNode> d(poll_state->takeOutput().get<BinaryNode>());
                bool ok = true;
                if (current_read_to_string) {
                    size_t len = d->size();
                    char* buf = reinterpret_cast<char*>(d->giveBuffer());
                    char* nbuf = reinterpret_cast<char*>(q_realloc(buf, len + 1));
                    if (!nbuf) {
                        xsink->outOfMemory();
                        ok = false;
                    } else {
                        nbuf[len] = '\0';
                        data = new QoreStringNode(nbuf, len, len + 1, sock->getEncoding());
                    }
                } else {
                    data = d.release();
                }
                poll_state.reset();
                priv->clearNonBlock(NB_RECV);
                set_non_block = false;
                if (!ok) {
                    chunked_body_state = ChunkedBodyState::DONE;
                    return nullptr;
                }
            }
        }

        if (poll_rc) {
            return getSocketPollInfoHash(xsink, poll_rc);
        }

        int rc = 0;
        switch (chunked_body_state) {
            case ChunkedBodyState::RECV_CHUNK_SIZE:
                rc = handleChunkSize(xsink);
                break;
            case ChunkedBodyState::RECV_CHUNK_DATA:
                rc = handleChunkData(xsink);
                break;
            case ChunkedBodyState::RECV_TRAILER:
                rc = handleTrailer(xsink);
                break;
            case ChunkedBodyState::DONE:
                received = true;
                return nullptr;
        }
        if (rc || *xsink) {
            chunked_body_state = ChunkedBodyState::DONE;
            return nullptr;
        }
    }

    return nullptr;
}

void SocketReadHttpChunkedBodyPollOperation::abort(ExceptionSink* xsink) {
    bool needs_close = abortNeedsClose();
    my_socket_priv* priv = my_socket_priv::getPriv(*sock);
    {
        AutoLocker al(priv->m);
        poll_state.reset();
        if (set_non_block) {
            priv->clearNonBlock(NB_RECV);
            set_non_block = false;
        }
        if (needs_close) {
            qore_socket_close_from_controller(priv->socket);
        }
    }
    out = nullptr;
    data.discard();
    chunked_body_state = ChunkedBodyState::DONE;
    received = true;
}

QoreValue SocketReadHttpChunkedBodyPollOperation::getOutput() const {
    return out.release();
}

const char* SocketReadHttpChunkedBodyPollOperation::getStateImpl() const {
    switch (chunked_body_state) {
        case ChunkedBodyState::RECV_CHUNK_SIZE:
            return "recv-chunk-size";
        case ChunkedBodyState::RECV_CHUNK_DATA:
            return "recv-chunk-data";
        case ChunkedBodyState::RECV_TRAILER:
            return "recv-chunk-trailer";
        case ChunkedBodyState::DONE:
            return "received";
        default:
            return "unknown";
    }
}

bool SocketReadHttpChunkedBodyPollOperation::goalReached() const {
    return received;
}

bool SocketReadHttpChunkedBodyPollOperation::abortNeedsClose() const {
    if (bytes_consumed) {
        return true;
    }
    if (!poll_state) {
        return false;
    }
    if (current_read_to_string) {
        assert(dynamic_cast<SocketRecvUntilBytesPollState*>(poll_state.get()));
        return reinterpret_cast<SocketRecvUntilBytesPollState*>(poll_state.get())->getBytesReceived() ? true : false;
    }
    assert(dynamic_cast<SocketRecvPollState*>(poll_state.get()));
    return reinterpret_cast<SocketRecvPollState*>(poll_state.get())->getBytesReceived() ? true : false;
}

// SocketReadHttpBodyPollOperation implementation

SocketReadHttpBodyPollOperation::SocketReadHttpBodyPollOperation(ExceptionSink* xsink,
        QoreSocketObject* sock, int status_code, int64_t content_length,
        bool chunked, bool connection_close, bool is_head)
        : SocketPollOperationBase(nullptr), sock_obj(sock), body_state(BodyState::DONE) {
    // sock is ref'd by caller

    // Determine if body exists
    bool no_body = is_head
        || (status_code >= 100 && status_code < 200)
        || status_code == 204
        || status_code == 304;

    if (no_body || (content_length == 0 && !chunked && !connection_close)) {
        return;
    }

    body = new BinaryNode();

    if (chunked) {
        body_state = BodyState::RECV_CHUNK_SIZE;
        startBodyRead(xsink);
    } else if (content_length > 0) {
        body_state = BodyState::RECV_LENGTH;
        remaining = content_length;
        startBodyRead(xsink);
    } else if (connection_close) {
        body_state = BodyState::RECV_CLOSE;
        startBodyRead(xsink);
    }
    // else: no Content-Length, not chunked, not connection-close = no body (already DONE)
}

SocketReadHttpBodyPollOperation::~SocketReadHttpBodyPollOperation() {
    // cleanup() should have been called via deref()
}

void SocketReadHttpBodyPollOperation::cleanup(ExceptionSink* xsink) {
    if (current_op) {
        current_op->deref(xsink);
        current_op = nullptr;
    }
    if (body) {
        body->deref(xsink);
        body = nullptr;
    }
    if (sock_obj) {
        sock_obj->deref(xsink);
        sock_obj = nullptr;
    }
}

void SocketReadHttpBodyPollOperation::releaseCurrentOp(ExceptionSink* xsink) {
    if (current_op) {
        current_op->deref(xsink);
        current_op = nullptr;
    }
}

void SocketReadHttpBodyPollOperation::startBodyRead(ExceptionSink* xsink) {
    sock_obj->ref();
    switch (body_state) {
        case BodyState::RECV_LENGTH:
            current_op = new SocketRecvPollOperation(xsink, (ssize_t)remaining, sock_obj, false);
            break;
        case BodyState::RECV_CHUNK_SIZE: {
            SimpleRefHolder<QoreStringNode> pattern(new QoreStringNode("\r\n"));
            current_op = new SocketRecvUntilBytesPollOperation(xsink, pattern.release(), sock_obj, true);
            break;
        }
        case BodyState::RECV_CLOSE:
            current_op = new SocketRecvDataPollOperation(xsink, sock_obj, false);
            break;
        default:
            sock_obj->deref(xsink);
            break;
    }
    if (*xsink && current_op) {
        current_op->deref(xsink);
        current_op = nullptr;
    }
    // Share parent's QoreObject self with inner op so getSocketPollInfoHash()
    // can access the "sock" member for SocketPollInfo creation.
    // Inner ops are raw C++ objects (not wrapped in their own QoreObject),
    // so they need the parent's self to produce valid poll info.
    if (current_op && self) {
        current_op->setSelf(*self);
    }
}

QoreHashNode* SocketReadHttpBodyPollOperation::continuePoll(ExceptionSink* xsink) {
    // Ensure current inner op has self set for getSocketPollInfoHash().
    // Inner ops created in the constructor don't have self yet (it's set after
    // construction via setSelf()), so propagate it on first continuePoll() call.
    if (current_op && self && !current_op->self) {
        current_op->setSelf(*self);
    }
    if (body_state == BodyState::DONE) {
        return nullptr;
    }

    while (true) {
        if (!current_op) {
            body_state = BodyState::DONE;
            return nullptr;
        }

        ExceptionSink poll_xsink;
        QoreHashNode* poll_info = current_op->continuePoll(&poll_xsink);
        if (poll_xsink) {
            if (body_state == BodyState::RECV_CLOSE) {
                // Read error in connection-close mode means EOF — body is complete
                poll_xsink.clear();
                body_state = BodyState::DONE;
                return nullptr;
            }
            xsink->assimilate(poll_xsink);
            body_state = BodyState::DONE;
            return nullptr;
        }

        if (poll_info) {
            return poll_info;
        }

        if (!current_op->goalReached()) {
            body_state = BodyState::DONE;
            return nullptr;
        }

        switch (body_state) {
            case BodyState::RECV_LENGTH:
                return handleRecvLength(xsink);
            case BodyState::RECV_CHUNK_SIZE:
                return handleRecvChunkSize(xsink);
            case BodyState::RECV_CHUNK_DATA:
                return handleRecvChunkData(xsink);
            case BodyState::RECV_CHUNK_CRLF:
                return handleRecvChunkCrlf(xsink);
            case BodyState::RECV_CLOSE:
                return handleRecvClose(xsink);
            default:
                return nullptr;
        }
    }
}

QoreHashNode* SocketReadHttpBodyPollOperation::handleRecvLength(ExceptionSink* xsink) {
    ValueHolder data(current_op->getOutput(), xsink);
    releaseCurrentOp(xsink);

    if (data->getType() == NT_BINARY) {
        const BinaryNode* bin = data->get<const BinaryNode>();
        if (bin->size() > 0) {
            body->append(bin->getPtr(), bin->size());
        }
    }

    body_state = BodyState::DONE;
    return nullptr;
}

QoreHashNode* SocketReadHttpBodyPollOperation::handleRecvChunkSize(ExceptionSink* xsink) {
    ValueHolder line_val(current_op->getOutput(), xsink);
    releaseCurrentOp(xsink);

    if (line_val->getType() != NT_STRING) {
        body_state = BodyState::DONE;
        return nullptr;
    }

    QoreStringValueHelper line(*line_val);
    const char* str = line->c_str();
    size_t len = line->size();

    // Strip trailing \r\n
    if (len >= 2) {
        len -= 2;
    }

    // Find semicolon (chunk extensions)
    const char* semi = (const char*)memchr(str, ';', len);
    size_t hex_len = semi ? (size_t)(semi - str) : len;

    // Parse hex chunk size
    char hex_buf[32];
    if (hex_len >= sizeof(hex_buf)) {
        hex_len = sizeof(hex_buf) - 1;
    }
    memcpy(hex_buf, str, hex_len);
    hex_buf[hex_len] = '\0';
    int64_t chunk_size = strtol(hex_buf, nullptr, 16);

    printd(5, "SocketReadHttpBodyPollOperation::handleRecvChunkSize() chunk_size=%lld\n",
        (long long)chunk_size);

    if (chunk_size == 0) {
        // Last chunk — read trailing CRLF (or trailers followed by CRLF)
        body_state = BodyState::RECV_CHUNK_CRLF;
        sock_obj->ref();
        SimpleRefHolder<QoreStringNode> pattern(new QoreStringNode("\r\n"));
        current_op = new SocketRecvUntilBytesPollOperation(xsink, pattern.release(), sock_obj, true);
        if (*xsink) {
            releaseCurrentOp(xsink);
            body_state = BodyState::DONE;
            return nullptr;
        }
        return continuePoll(xsink);
    }

    // Read chunk data + trailing CRLF (chunk_size + 2 bytes)
    body_state = BodyState::RECV_CHUNK_DATA;
    sock_obj->ref();
    current_op = new SocketRecvPollOperation(xsink, (ssize_t)(chunk_size + 2), sock_obj, false);
    if (*xsink) {
        releaseCurrentOp(xsink);
        body_state = BodyState::DONE;
        return nullptr;
    }
    return continuePoll(xsink);
}

QoreHashNode* SocketReadHttpBodyPollOperation::handleRecvChunkData(ExceptionSink* xsink) {
    ValueHolder data_val(current_op->getOutput(), xsink);
    releaseCurrentOp(xsink);

    if (data_val->getType() == NT_BINARY) {
        const BinaryNode* bin = data_val->get<const BinaryNode>();
        // Remove trailing CRLF (last 2 bytes)
        if (bin->size() > 2) {
            body->append(bin->getPtr(), bin->size() - 2);
        }
    }

    // Read next chunk size
    body_state = BodyState::RECV_CHUNK_SIZE;
    sock_obj->ref();
    SimpleRefHolder<QoreStringNode> pattern(new QoreStringNode("\r\n"));
    current_op = new SocketRecvUntilBytesPollOperation(xsink, pattern.release(), sock_obj, true);
    if (*xsink) {
        releaseCurrentOp(xsink);
        body_state = BodyState::DONE;
        return nullptr;
    }
    return continuePoll(xsink);
}

QoreHashNode* SocketReadHttpBodyPollOperation::handleRecvChunkCrlf(ExceptionSink* xsink) {
    ValueHolder line_val(current_op->getOutput(), xsink);
    releaseCurrentOp(xsink);

    if (line_val->getType() == NT_STRING) {
        QoreStringValueHelper line(*line_val);
        if (line->size() == 2 && !strcmp(line->c_str(), "\r\n")) {
            // Empty line — chunked transfer complete
            body_state = BodyState::DONE;
            return nullptr;
        }
        // Trailer line — read next line
        printd(5, "SocketReadHttpBodyPollOperation::handleRecvChunkCrlf() trailer line\n");
        sock_obj->ref();
        SimpleRefHolder<QoreStringNode> pattern(new QoreStringNode("\r\n"));
        current_op = new SocketRecvUntilBytesPollOperation(xsink, pattern.release(), sock_obj, true);
        if (*xsink) {
            releaseCurrentOp(xsink);
            body_state = BodyState::DONE;
            return nullptr;
        }
        return continuePoll(xsink);
    }

    // Unexpected output type — treat as complete
    body_state = BodyState::DONE;
    return nullptr;
}

QoreHashNode* SocketReadHttpBodyPollOperation::handleRecvClose(ExceptionSink* xsink) {
    ValueHolder data_val(current_op->getOutput(), xsink);
    releaseCurrentOp(xsink);

    bool has_data = false;
    if (data_val->getType() == NT_BINARY) {
        const BinaryNode* bin = data_val->get<const BinaryNode>();
        if (bin->size() > 0) {
            has_data = true;
            body->append(bin->getPtr(), bin->size());
        }
    }

    if (has_data) {
        // Read more data
        sock_obj->ref();
        current_op = new SocketRecvDataPollOperation(xsink, sock_obj, false);
        if (*xsink) {
            releaseCurrentOp(xsink);
            body_state = BodyState::DONE;
            return nullptr;
        }
        return continuePoll(xsink);
    }

    // Empty read = EOF — server closed connection
    body_state = BodyState::DONE;
    return nullptr;
}

void SocketReadHttpBodyPollOperation::abort(ExceptionSink* xsink) {
    if (current_op) {
        current_op->abort(xsink);
    }
    body_state = BodyState::DONE;
}

QoreValue SocketReadHttpBodyPollOperation::getOutput() const {
    if (body) {
        body->ref();
        return body;
    }
    return QoreValue();
}

const char* SocketReadHttpBodyPollOperation::getStateImpl() const {
    switch (body_state) {
        case BodyState::RECV_LENGTH: return "recv_body_length";
        case BodyState::RECV_CHUNK_SIZE: return "recv_chunk_size";
        case BodyState::RECV_CHUNK_DATA: return "recv_chunk_data";
        case BodyState::RECV_CHUNK_CRLF: return "recv_chunk_crlf";
        case BodyState::RECV_CLOSE: return "recv_body_close";
        case BodyState::DONE: return "done";
        default: return "unknown";
    }
}

bool SocketReadHttpBodyPollOperation::goalReached() const {
    return body_state == BodyState::DONE;
}

void SocketReadHttpBodyPollOperation::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        cleanup(xsink);
        delete this;
    }
}

// SocketSendAndReadHeaderPollOperation implementation

SocketSendAndReadHeaderPollOperation::SocketSendAndReadHeaderPollOperation(ExceptionSink* xsink,
        BinaryNode* response_data, QoreSocketObject* sock, int64 idle_timeout_ms)
        : SocketPollSocketOperationBase(sock), send_data(response_data),
          buf(reinterpret_cast<const char*>(response_data->getPtr())),
          size(response_data->size()),
          idle_timeout_ms(idle_timeout_ms), header_output(xsink) {
    AutoLocker al(sock->priv->m);

    // throw an exception and exit if the object is no longer open or valid
    if (sock->priv->checkOpen(xsink)) {
        return;
    }

    if (!sock->priv->setNonBlock(xsink)) {
        poll_state.reset(sock->priv->socket->startSend(xsink, buf, size));
        if (!poll_state) {
            sock->priv->clearNonBlock();
        } else {
            set_non_block = true;
        }
    }
}

const char* SocketSendAndReadHeaderPollOperation::getStateImpl() const {
    switch (phase) {
        case Phase::Sending: return "sending";
        case Phase::Idle: return "idle";
        case Phase::ReadingHeader: return "reading-header";
        case Phase::Complete: return "header-complete";
        case Phase::Timeout: return "timeout";
        case Phase::Closed: return "closed";
        case Phase::Error: return "error";
        default: return "unknown";
    }
}

QoreHashNode* SocketSendAndReadHeaderPollOperation::continuePoll(ExceptionSink* xsink) {
    AutoLocker al(sock->priv->m);
    if (sock->priv->checkOpen(xsink)) {
        printd(5, "SocketSendAndReadHeaderPollOperation::continuePoll() socket NOT open, phase=%d\n", (int)phase);
        phase = Phase::Error;
        return nullptr;
    }

    switch (phase) {
        case Phase::Sending: {
            if (!poll_state) {
                phase = Phase::Error;
                return nullptr;
            }
            // Drive the send poll state
            int rc = poll_state->continuePoll(xsink);
            if (*xsink) {
                phase = Phase::Error;
                poll_state.reset();
                return nullptr;
            }
            if (rc) {
                // Need more I/O for sending
                return getSocketPollInfoHash(xsink, rc);
            }
            // Send complete — free send data and poll state, transition to idle
            poll_state.reset();
            send_data.discard();
            phase = Phase::Idle;
            // Return POLLIN to wait for incoming data
            return getSocketPollInfoHash(xsink, SOCK_POLLIN);
        }

        case Phase::Idle: {
            // Check idle timeout
            int us;
            int64 now_s = q_epoch_us(us);
            int64 now_ms = now_s * 1000 + us / 1000;
            if (idle_timeout_ms > 0 && now_ms > idle_timeout_ms) {
                phase = Phase::Timeout;
                sock->priv->clearNonBlock();
                set_non_block = false;
                return nullptr;
            }
            int idle_rc = sock->checkIdleDataForAsyncPollLocked(xsink);
            if (*xsink || idle_rc < 0) {
                phase = Phase::Closed;
                qore_socket_close_from_controller(sock->priv->socket);
                sock->priv->clearNonBlock();
                set_non_block = false;
                return nullptr;
            }
            if (!idle_rc) {
                return getSocketPollInfoHash(xsink, SOCK_POLLIN);
            }
            // Data is available — transition to header reading
            poll_state.reset(sock->priv->socket->startRecvUntilBytes(xsink, "\r\n\r\n", 4));
            if (*xsink || !poll_state) {
                phase = Phase::Error;
                sock->priv->clearNonBlock();
                set_non_block = false;
                return nullptr;
            }
            phase = Phase::ReadingHeader;
        }
        // fall through to drive header read immediately

        case Phase::ReadingHeader: {
            if (!poll_state) {
                phase = Phase::Error;
                return nullptr;
            }
            int rc = poll_state->continuePoll(xsink);
            if (*xsink) {
                phase = Phase::Error;
                poll_state.reset();
                sock->priv->clearNonBlock();
                set_non_block = false;
                return nullptr;
            }
            if (rc) {
                // Need more I/O for header reading
                return getSocketPollInfoHash(xsink, rc);
            }
            // Header read complete — parse it
            SimpleRefHolder<BinaryNode> raw(poll_state->takeOutput().get<BinaryNode>());
            poll_state.reset();

            // Convert binary to string for HTTP header parsing
            size_t len = raw->size();
            char* buf = reinterpret_cast<char*>(raw->giveBuffer());
            char* nbuf = reinterpret_cast<char*>(q_realloc(buf, len + 1));
            if (!nbuf) {
                free(buf);
                xsink->outOfMemory();
                phase = Phase::Error;
                sock->priv->clearNonBlock();
                set_non_block = false;
                return nullptr;
            }
            nbuf[len] = '\0';
            SimpleRefHolder<QoreStringNode> hdrstr(new QoreStringNode(nbuf, len, len + 1, sock->getEncoding()));

            // Parse HTTP headers
            ReferenceHolder<QoreHashNode> info(new QoreHashNode(autoTypeInfo), xsink);
            ReferenceHolder<QoreHashNode> hdr(sock->priv->socket->priv->processHttpHeaderString(xsink, hdrstr,
                *info, QORE_SOURCE_SOCKET), xsink);
            if (*xsink) {
                phase = Phase::Error;
                sock->priv->clearNonBlock();
                set_non_block = false;
                return nullptr;
            }

            // Store result for getOutput()
            header_output = new QoreHashNode(autoTypeInfo);
            header_output->setKeyValue("hdr", hdr.release(), xsink);
            header_output->setKeyValue("info", info.release(), xsink);

            // Clear non-block mode now that we're done
            sock->priv->clearNonBlock();
            set_non_block = false;

            phase = Phase::Complete;
            return nullptr;
        }

        default:
            return nullptr;
    }
}

QoreValue SocketSendAndReadHeaderPollOperation::getOutput() const {
    AutoLocker al(sock->priv->m);
    return header_output.release();
}

SocketSendStreamAndReadHeaderPollOperation::SocketSendStreamAndReadHeaderPollOperation(ExceptionSink* xsink,
        BinaryNode* hdr_data, QoreSocketObject* sock, InputStream* is, QoreObject* is_obj,
        int64 content_length, int64 idle_timeout_ms, bool fused, bool chunked)
        : SocketPollSocketOperationBase(sock), header_data(hdr_data),
          hdr_buf(reinterpret_cast<const char*>(hdr_data->getPtr())),
          hdr_size(hdr_data->size()),
          input_stream(is),
          input_stream_obj(is_obj),
          content_length(content_length),
          chunked_encoding(chunked),
          fused(fused),
          idle_timeout_ms(idle_timeout_ms), header_output(xsink) {

    if (!is->isIoThreadSafe()) {
        xsink->raiseException("HTTP-STREAM-ERROR", "InputStream is not I/O thread safe");
        return;
    }

    stream_fd = qore_input_stream_pollable_descriptor(is, "HTTP-STREAM-ERROR", xsink);
    if (*xsink) {
        return;
    }
    is_pollable = stream_fd >= 0;

    AutoLocker al(sock->priv->m);

    // Throw an exception and exit if the object is no longer open or valid
    if (sock->priv->checkOpen(xsink)) {
        return;
    }

    if (!sock->priv->setNonBlock(xsink)) {
        poll_state.reset(sock->priv->socket->startSend(xsink, hdr_buf, hdr_size));
        if (!poll_state) {
            sock->priv->clearNonBlock();
        } else {
            set_non_block = true;
        }
    }
}

const char* SocketSendStreamAndReadHeaderPollOperation::getStateImpl() const {
    switch (phase) {
        case Phase::SendHeaders: return "sending-headers";
        case Phase::StreamBody: return "streaming-body";
        case Phase::Idle: return "idle";
        case Phase::ReadingHeader: return "reading-header";
        case Phase::Complete: return "stream-complete";
        case Phase::Timeout: return "timeout";
        case Phase::Closed: return "closed";
        case Phase::Error: return "error";
        default: return "unknown";
    }
}

QoreHashNode* SocketSendStreamAndReadHeaderPollOperation::continuePoll(ExceptionSink* xsink) {
    SafeLocker al(sock->priv->m);
    if (sock->priv->checkOpen(xsink)) {
        phase = Phase::Error;
        return nullptr;
    }

    while (true) {
        switch (phase) {
            case Phase::SendHeaders: {
                if (!poll_state) {
                    phase = Phase::Error;
                    return nullptr;
                }
                // Drive the send poll state
                int rc = poll_state->continuePoll(xsink);
                if (*xsink) {
                    phase = Phase::Error;
                    poll_state.reset();
                    return nullptr;
                }
                if (rc) {
                    // Need more I/O for sending
                    return getSocketPollInfoHash(xsink, rc);
                }
                // Send complete — free send data and poll state, transition to StreamBody
                poll_state.reset();
                header_data.discard();
                phase = Phase::StreamBody;
                // If stream body has data to send, continue immediately
                continue;
            }

            case Phase::StreamBody: {
                // Handle InputStream thread reassignment (must be outside socket lock)
                if (need_reassign) {
                    need_reassign = false;
                    al.unlock();
                    if (input_stream) {
                        input_stream->reassignThread(xsink);
                        if (*xsink) {
                            phase = Phase::Error;
                            return nullptr;
                        }
                    }
                    al.relock();
                    if (sock->priv->checkOpen(xsink)) {
                        phase = Phase::Error;
                        return nullptr;
                    }
                }

                // If we have a chunk currently being sent, drive the send
                if (poll_state) {
                    int rc = poll_state->continuePoll(xsink);
                    if (*xsink) {
                        phase = Phase::Error;
                        poll_state.reset();
                        return nullptr;
                    }
                    if (rc) {
                        // Need more socket I/O for sending the current chunk.
                        return getSocketPollInfoHash(xsink, rc);
                    }
                    // Chunk sent successfully
                    poll_state.reset();
                    bytes_sent += current_chunk->size();
                    current_chunk = nullptr;
                }

                // Check if we've sent all the data
                if (chunked_encoding ? sent_terminal_chunk : (bytes_sent >= content_length)) {
                    // All data sent; unassign InputStream thread
                    if (input_stream) {
                        input_stream->unassignThread(xsink);
                        input_stream = nullptr;
                    }
                    if (fused) {
                        phase = Phase::Idle;
                        return getSocketPollInfoHash(xsink, SOCK_POLLIN);
                    }
                    // Non-fused: complete
                    sock->priv->clearNonBlock();
                    set_non_block = false;
                    phase = Phase::Complete;
                    return nullptr;
                }

                // Read the next chunk from InputStream
                int64 chunk_size;
                if (chunked_encoding) {
                    chunk_size = 65536;  // Read up to 64KB per chunk
                } else {
                    int64 remaining = content_length - bytes_sent;
                    chunk_size = remaining < 65536 ? remaining : 65536;
                }

                if (is_pollable) {
                    // Non-blocking read for pollable streams.
                    // Probe readiness without blocking,
                    // then readNonBlock() to get the data. This distinguishes:
                    // - poll ready + readNonBlock returns 0 → true EOF
                    // - poll not ready → would block, yield to event loop
                    assert(stream_fd >= 0);
                    int poll_rv = qore_socket_poll_fd_now(stream_fd, POLLIN, "HTTP-STREAM-ERROR", "stream", xsink);
                    if (poll_rv < 0) {
                        phase = Phase::Error;
                        return nullptr;
                    }
                    if (poll_rv == 0) {
                        // Stream not ready — register fd with event loop
                        std::vector<std::pair<int, int>> extra_fds{{stream_fd, SOCK_POLLIN}};
                        return getSocketPollInfoHash(xsink, 0, extra_fds);
                    }

                    // Stream FD is readable — do non-blocking read
                    SimpleRefHolder<BinaryNode> chunk(new BinaryNode);
                    chunk->preallocate(chunk_size);
                    int64 count = input_stream->readNonBlock(
                        const_cast<void*>(chunk->getPtr()), chunk_size, xsink);
                    if (*xsink) {
                        phase = Phase::Error;
                        return nullptr;
                    }
                    if (count <= 0) {
                        // poll said readable but readNonBlock returned 0 → true EOF
                        if (input_stream) {
                            input_stream->unassignThread(xsink);
                            input_stream = nullptr;
                        }
                        if (chunked_encoding) {
                            // Send terminal chunk "0\r\n\r\n"
                            SimpleRefHolder<BinaryNode> term(new BinaryNode);
                            term->append("0\r\n\r\n", 5);
                            current_chunk = term.release();
                            sent_terminal_chunk = true;
                            // Fall through to start sending
                        } else {
                            if (bytes_sent < content_length) {
                                xsink->raiseException("HTTP-STREAM-ERROR",
                                    "InputStream EOF after " QLLD " bytes but Content-Length is " QLLD,
                                    bytes_sent, content_length);
                                phase = Phase::Error;
                                return nullptr;
                            }
                            // bytes_sent == content_length: normal completion
                            if (fused) {
                                phase = Phase::Idle;
                                return getSocketPollInfoHash(xsink, SOCK_POLLIN);
                            }
                            sock->priv->clearNonBlock();
                            set_non_block = false;
                            phase = Phase::Complete;
                            return nullptr;
                        }
                    } else {
                        chunk->setSize(count);
                        if (chunked_encoding) {
                            // Frame as HTTP chunked: "<hex-size>\r\n<data>\r\n"
                            QoreString hex;
                            hex.sprintf("%x\r\n", (int)count);
                            SimpleRefHolder<BinaryNode> framed(new BinaryNode);
                            framed->append(hex.c_str(), hex.size());
                            framed->append(chunk->getPtr(), count);
                            framed->append("\r\n", 2);
                            current_chunk = framed.release();
                        } else {
                            current_chunk = chunk.release();
                        }
                    }
                } else {
                    // Non-pollable (memory streams) — readHelper never blocks
                    SimpleRefHolder<BinaryNode> raw_chunk(
                        input_stream->readHelper(chunk_size, xsink));
                    if (*xsink) {
                        phase = Phase::Error;
                        return nullptr;
                    }
                    if (!raw_chunk || !raw_chunk->size()) {
                        // EOF from non-pollable InputStream
                        if (input_stream) {
                            input_stream->unassignThread(xsink);
                            input_stream = nullptr;
                        }
                        if (chunked_encoding) {
                            // Send terminal chunk
                            SimpleRefHolder<BinaryNode> term(new BinaryNode);
                            term->append("0\r\n\r\n", 5);
                            current_chunk = term.release();
                            sent_terminal_chunk = true;
                            // Fall through to start sending
                        } else {
                            if (bytes_sent < content_length) {
                                xsink->raiseException("HTTP-STREAM-ERROR",
                                    "InputStream provided " QLLD " bytes but Content-Length is " QLLD,
                                    bytes_sent, content_length);
                                phase = Phase::Error;
                                return nullptr;
                            }
                            // bytes_sent == content_length: normal completion
                            if (fused) {
                                phase = Phase::Idle;
                                return getSocketPollInfoHash(xsink, SOCK_POLLIN);
                            }
                            sock->priv->clearNonBlock();
                            set_non_block = false;
                            phase = Phase::Complete;
                            return nullptr;
                        }
                    } else if (chunked_encoding) {
                        // Frame as HTTP chunked
                        QoreString hex;
                        hex.sprintf("%x\r\n", (int)raw_chunk->size());
                        SimpleRefHolder<BinaryNode> framed(new BinaryNode);
                        framed->append(hex.c_str(), hex.size());
                        framed->append(raw_chunk->getPtr(), raw_chunk->size());
                        framed->append("\r\n", 2);
                        current_chunk = framed.release();
                    } else {
                        current_chunk = raw_chunk.release();
                    }
                }

                // Start sending the chunk
                poll_state.reset(sock->priv->socket->startSend(xsink,
                    reinterpret_cast<const char*>(current_chunk->getPtr()), current_chunk->size()));
                if (*xsink || !poll_state) {
                    phase = Phase::Error;
                    return nullptr;
                }
                // Drive the send immediately
                continue;
            }

            case Phase::Idle: {
                // Check idle timeout
                int us;
                int64 now_s = q_epoch_us(us);
                int64 now_ms = now_s * 1000 + us / 1000;
                if (idle_timeout_ms > 0 && now_ms > idle_timeout_ms) {
                    phase = Phase::Timeout;
                    sock->priv->clearNonBlock();
                    set_non_block = false;
                    return nullptr;
                }
                int idle_rc = sock->checkIdleDataForAsyncPollLocked(xsink);
                if (*xsink || idle_rc < 0) {
                    phase = Phase::Closed;
                    qore_socket_close_from_controller(sock->priv->socket);
                    sock->priv->clearNonBlock();
                    set_non_block = false;
                    return nullptr;
                }
                if (!idle_rc) {
                    return getSocketPollInfoHash(xsink, SOCK_POLLIN);
                }
                // Data is available — transition to header reading
                poll_state.reset(sock->priv->socket->startRecvUntilBytes(xsink, "\r\n\r\n", 4));
                if (*xsink || !poll_state) {
                    phase = Phase::Error;
                    sock->priv->clearNonBlock();
                    set_non_block = false;
                    return nullptr;
                }
                phase = Phase::ReadingHeader;
            }
            // fall through to drive header read immediately

            case Phase::ReadingHeader: {
                if (!poll_state) {
                    phase = Phase::Error;
                    return nullptr;
                }
                int rc = poll_state->continuePoll(xsink);
                if (*xsink) {
                    phase = Phase::Error;
                    poll_state.reset();
                    sock->priv->clearNonBlock();
                    set_non_block = false;
                    return nullptr;
                }
                if (rc) {
                    // Need more I/O for header reading
                    return getSocketPollInfoHash(xsink, rc);
                }
                // Header read complete — parse it
                SimpleRefHolder<BinaryNode> raw(poll_state->takeOutput().get<BinaryNode>());
                poll_state.reset();

                // Convert binary to string for HTTP header parsing
                size_t len = raw->size();
                char* buf = reinterpret_cast<char*>(raw->giveBuffer());
                char* nbuf = reinterpret_cast<char*>(q_realloc(buf, len + 1));
                if (!nbuf) {
                    free(buf);
                    xsink->outOfMemory();
                    phase = Phase::Error;
                    sock->priv->clearNonBlock();
                    set_non_block = false;
                    return nullptr;
                }
                nbuf[len] = '\0';
                SimpleRefHolder<QoreStringNode> hdrstr(
                    new QoreStringNode(nbuf, len, len + 1, sock->getEncoding()));

                // Parse HTTP headers
                ReferenceHolder<QoreHashNode> info(new QoreHashNode(autoTypeInfo), xsink);
                ReferenceHolder<QoreHashNode> hdr(
                    sock->priv->socket->priv->processHttpHeaderString(xsink, hdrstr,
                        *info, QORE_SOURCE_SOCKET), xsink);
                if (*xsink) {
                    phase = Phase::Error;
                    sock->priv->clearNonBlock();
                    set_non_block = false;
                    return nullptr;
                }

                // Store result for getOutput()
                header_output = new QoreHashNode(autoTypeInfo);
                header_output->setKeyValue("hdr", hdr.release(), xsink);
                header_output->setKeyValue("info", info.release(), xsink);

                // Clear non-block mode now that we're done
                sock->priv->clearNonBlock();
                set_non_block = false;

                phase = Phase::Complete;
                return nullptr;
            }

            default:
                return nullptr;
        }
    }
}

QoreValue SocketSendStreamAndReadHeaderPollOperation::getOutput() const {
    AutoLocker al(sock->priv->m);
    return header_output.release();
}

#include "qore/intern/Http2Session.h"

SocketHttp2ServerPollOperation::SocketHttp2ServerPollOperation(ExceptionSink* xsink, QoreSocketObject* sock)
        : SocketPollSocketOperationBase(sock) {
    AutoLocker al(sock->priv->m);

    // Check if socket is open and valid
    if (sock->priv->checkOpen(xsink)) {
        return;
    }

    // Set non-blocking mode
    if (sock->priv->setNonBlock(xsink)) {
        return;
    }
    set_non_block = true;

    // Check if there's already an HTTP/2 session stored in the socket (from a previous request)
    bool reused_session = false;
    if (sock->priv->socket->priv->h2_session) {
        // Reuse existing session (socket owns it)
        h2_session = sock->priv->socket->priv->h2_session;
        // Update max body size limit in case it changed since session was created
        h2_session->setMaxRequestBodySize(
            sock->priv->socket->priv->max_http2_body_size.load(std::memory_order_relaxed));
        reused_session = true;
    } else {
        // Initialize new HTTP/2 session and store it on the socket
        if (initSession(xsink)) {
            sock->priv->clearNonBlock();
            set_non_block = false;
            return;
        }
    }

    // Set initial state based on whether we reused a session
    if (reused_session) {
        // Session already established - go directly to reading frames
        h2_state = H2S_READING;
    } else {
        // New session - need to exchange connection preface
        h2_state = H2S_SEND_PREFACE;
    }
}

SocketHttp2ServerPollOperation::~SocketHttp2ServerPollOperation() {
}

int SocketHttp2ServerPollOperation::initSession(ExceptionSink* xsink) {
    // Create server-side HTTP/2 session using the underlying QoreSocket's priv
    h2_session = Http2Session::createServer(sock->priv->socket->priv, xsink);
    if (!h2_session) {
        return -1;
    }
    // Apply socket-level HTTP/2 settings before the connection preface is sent
    h2_session->setEnableConnectProtocol(sock->priv->socket->priv->h2_enable_connect_protocol);
    // Propagate max request body size limit to the HTTP/2 session
    int64 max_http2_body_size = sock->priv->socket->priv->max_http2_body_size.load(std::memory_order_relaxed);
    if (max_http2_body_size > 0) {
        h2_session->setMaxRequestBodySize(max_http2_body_size);
    }
    // Store session on socket for shared access
    sock->priv->socket->priv->h2_session = h2_session;
    return 0;
}

QoreHashNode* SocketHttp2ServerPollOperation::checkHeadersOnlyDispatch(bool& handled,
        ExceptionSink* xsink) {
    handled = false;
    if (!headers_only) {
        return nullptr;
    }
    // Flush pending protocol responses (SETTINGS_ACK, WINDOW_UPDATE) so the
    // client can proceed with sending HEADERS/DATA
    int srv = h2_session->sendPendingData(0, xsink);
    if (*xsink) {
        return nullptr;
    }
    if (srv == SOCK_POLLIN || srv == SOCK_POLLOUT) {
        handled = true;
        return getSocketPollInfoHash(xsink, srv);
    }
    // Atomically find a headers-ready stream, copy it, and mark dispatched
    cached_stream = h2_session->takeHeadersReadyStreamCopy();
    if (!cached_stream) {
        // No headers-ready stream yet
        return nullptr;
    }
    handled = true;
    h2_state = H2S_HEADERS_READY;
    if (set_non_block) {
        AutoLocker al(sock->priv->m);
        if (set_non_block) {
            set_non_block = false;
            sock->priv->clearNonBlock();
        }
    }
    return nullptr;
}

QoreHashNode* SocketHttp2ServerPollOperation::continuePoll(ExceptionSink* xsink) {
    auto clear_non_block = [&]() {
        if (set_non_block) {
            AutoLocker al(sock->priv->m);
            if (set_non_block) {
                set_non_block = false;
                sock->priv->clearNonBlock();
            }
        }
    };

    auto set_non_block_if_needed = [&]() -> bool {
        if (!set_non_block) {
            AutoLocker al(sock->priv->m);
            if (!set_non_block) {
                if (sock->priv->setNonBlock(xsink)) {
                    return true;
                }
                set_non_block = true;
            }
        }
        return false;
    };

    {
        AutoLocker al(sock->priv->m);
        if (!sock->priv->socket->priv->isOpen()) {
            xsink->raiseException("HTTP2-ERROR", "socket closed during poll operation");
            return nullptr;
        }
    }

    // Always flush pending data first (e.g., response frames queued by handler
    // thread via submitHttp2Response + wakeSocket, or SETTINGS_ACK / WINDOW_UPDATE
    // generated during prior frame processing).  Without this, responses sit in
    // nghttp2's send buffer until the next incoming data triggers a read.
    if (h2_session && (h2_session->hasPendingData() || h2_session->wantWrite())) {
        h2_session->sendPendingData(0, xsink);
        if (*xsink) {
            return nullptr;
        }
    }

    while (true) {
        switch (h2_state) {
            case H2S_SEND_PREFACE: {
                // Send server connection preface (SETTINGS frame)
                int rv = h2_session->sendConnectionPrefaceNonBlocking(xsink);
                if (*xsink) {
                    return nullptr;
                }
                if (rv < 0) {
                    return nullptr;
                }
                if (rv == SOCK_POLLIN || rv == SOCK_POLLOUT) {
                    return getSocketPollInfoHash(xsink, rv);
                }
                h2_state = H2S_RECV_PREFACE;
                // Fall through to receive preface
            }

            case H2S_RECV_PREFACE:
            case H2S_READING: {
                // Headers-only mode: check for streams with HEADERS complete but not yet dispatched
                {
                    bool handled = false;
                    QoreHashNode* rv = checkHeadersOnlyDispatch(handled, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (handled) {
                        return rv;
                    }
                }

                // If another thread already completed a stream, return it immediately
                if (h2_session->hasCompletedStreams()) {
                    // Flush any pending output (RST_STREAM, SETTINGS_ACK, etc.) before returning
                    int srv = h2_session->sendPendingData(0, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (srv == SOCK_POLLIN || srv == SOCK_POLLOUT) {
                        // Socket buffer full; poll and retry before completing the operation
                        return getSocketPollInfoHash(xsink, srv);
                    }
                    // Dequeue the completed stream now so getOutput() is idempotent
                    cached_stream = h2_session->takeCompletedStream();
                    h2_state = H2S_REQUEST_READY;
                    clear_non_block();
                    return nullptr;
                }
                // Try to receive data
                int rv = h2_session->receiveData(0, xsink);
                printd(5, "H2S_READING: receiveData rv=%d hasActiveIS=%d\n",
                    rv, h2_session->hasActiveStreamInputStreams());
                if (*xsink) {
                    return nullptr;
                }
                // Read from any pending InputStreams and submit data to session
                // (must happen regardless of receiveData result — handler threads
                // may have registered InputStreams via submitHttp2StreamingResponseWithStream)
                if (h2_session->hasActiveStreamInputStreams()) {
                    h2_session->processStreamInputStreams(xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                }

                if (rv == -1) {
                    // Would block on recv - also try to send pending response data
                    // (responses may have been queued by handler threads via submitResponse)
                    int srv = h2_session->sendPendingData(0, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (srv == SOCK_POLLIN || srv == SOCK_POLLOUT) {
                        return getSocketPollInfoHash(xsink, SOCK_POLLIN | srv);
                    }
                    // Always read; add write if there are pending sends
                    int events = SOCK_POLLIN;
                    if (h2_session->hasPendingData() || h2_session->wantWrite()
                            || h2_session->hasActiveStreamInputStreams()) {
                        events |= SOCK_POLLOUT;
                    }

                    // Collect extra fds from active pollable InputStreams
                    std::vector<std::pair<int, int>> extra_fds;
                    h2_session->getExtraFds(extra_fds);
                    if (!extra_fds.empty()) {
                        return getSocketPollInfoHash(xsink, events, extra_fds);
                    }
                    return getSocketPollInfoHash(xsink, events);
                }
                if (rv == 1) {
                    // Connection closed by peer - check if we have a completed request first
                    if (!h2_session->hasCompletedStreams()) {
                        peer_closed = true;
                        // Do NOT set H2S_REQUEST_READY: there are no completed streams to
                        // return, so goalReached() should return false.  The caller will see
                        // continuePoll() -> nullptr with goalReached() == false and handle
                        // the connection closure.
                        clear_non_block();
                        return nullptr;
                    }
                    // Fall through to handle the completed request
                }

                // Headers-only mode: check for headers-ready streams after receiving data
                {
                    bool handled = false;
                    QoreHashNode* hrv = checkHeadersOnlyDispatch(handled, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (handled) {
                        return hrv;
                    }
                }

                // Check if we have a completed request
                if (h2_session->hasCompletedStreams()) {
                    // Flush any pending output (RST_STREAM, SETTINGS_ACK, etc.) before returning
                    int srv = h2_session->sendPendingData(0, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (srv == SOCK_POLLIN || srv == SOCK_POLLOUT) {
                        // Socket buffer full; poll and retry before completing the operation
                        return getSocketPollInfoHash(xsink, srv);
                    }
                    // Dequeue the completed stream now so getOutput() is idempotent
                    cached_stream = h2_session->takeCompletedStream();
                    h2_state = H2S_REQUEST_READY;
                    // Goal reached - clear non-block flag so subsequent operations can proceed
                    clear_non_block();
                    return nullptr;
                }

                // Need to send any pending data (like SETTINGS ACK)
                rv = h2_session->sendPendingData(0, xsink);
                if (*xsink) {
                    return nullptr;
                }
                // sendPendingData returns:
                //   0: success
                //   SOCK_POLLIN: need to poll for read (TLS renegotiation)
                //   SOCK_POLLOUT: need to poll for write
                //   -1: error (exception set)
                if (rv == SOCK_POLLIN || rv == SOCK_POLLOUT) {
                    return getSocketPollInfoHash(xsink, SOCK_POLLIN | rv);
                }

                // If we were receiving preface, move to reading state
                if (h2_state == H2S_RECV_PREFACE) {
                    h2_state = H2S_READING;
                }

                // Always include POLLIN to continue reading new requests.
                // Add POLLOUT when there are pending response sends or active
                // InputStreams that need to be read and flushed.
                {
                    int events = SOCK_POLLIN;
                    if (h2_session->hasPendingData() || h2_session->wantWrite()
                            || h2_session->hasActiveStreamInputStreams()) {
                        events |= SOCK_POLLOUT;
                    }

                    // Collect extra fds from active pollable InputStreams
                    std::vector<std::pair<int, int>> extra_fds;
                    h2_session->getExtraFds(extra_fds);
                    if (!extra_fds.empty()) {
                        return getSocketPollInfoHash(xsink, events, extra_fds);
                    }
                    return getSocketPollInfoHash(xsink, events);
                }
            }

            case H2S_REQUEST_READY:
                // If there are more completed streams, dequeue the next one
                if (h2_session->hasCompletedStreams()) {
                    cached_stream = h2_session->takeCompletedStream();
                    clear_non_block();
                    return nullptr;
                }
                // All completed streams consumed - transition back to reading
                // so the same operation can be reused for multiplexing
                cached_stream.reset();
                h2_state = H2S_READING;
                if (set_non_block_if_needed()) {
                    return nullptr;
                }
                continue;

            case H2S_HEADERS_READY:
                // Headers-only dispatch complete - transition back to reading
                // so the operation can be reused for the next request
                cached_stream.reset();
                h2_state = H2S_READING;
                if (set_non_block_if_needed()) {
                    return nullptr;
                }
                continue;

            default:
                xsink->raiseException("HTTP2-ERROR", "unexpected state: %d", h2_state);
                return nullptr;
        }
    }
}

QoreValue SocketHttp2ServerPollOperation::getOutput() const {
    if (h2_state != H2S_REQUEST_READY && h2_state != H2S_HEADERS_READY) {
        return QoreValue();
    }
    if (peer_closed) {
        return QoreValue();
    }
    // cached_stream is dequeued in continuePoll() when transitioning to H2S_REQUEST_READY
    if (!cached_stream) {
        return QoreValue();
    }
    // Skip streams reset at protocol level (e.g., CONNECT without ENABLE_CONNECT_PROTOCOL)
    // The RST_STREAM was already sent by nghttp2 via sendPendingData()
    if (cached_stream->reset) {
        return QoreValue();
    }

    const_cast<SocketHttp2ServerPollOperation*>(this)->stream_id = cached_stream->stream_id;

    // Build output hash from cached stream info (idempotent - safe to call multiple times)
    QoreHashNode* result = new QoreHashNode(autoTypeInfo);
    result->setKeyValue("method", new QoreStringNode(cached_stream->method), nullptr);
    result->setKeyValue("path", new QoreStringNode(cached_stream->path), nullptr);
    result->setKeyValue("stream_id", cached_stream->stream_id, nullptr);

    // Build headers hash (lowercase names, handle duplicate headers per RFC 7540)
    QoreHashNode* headers = httpMultiHeadersToQoreHash(cached_stream->headers, true);
    result->setKeyValue("headers", headers, nullptr);

    // Body as binary
    if (!cached_stream->body.empty()) {
        BinaryNode* body = new BinaryNode();
        body->append(cached_stream->body.data(), cached_stream->body.size());
        result->setKeyValue("body", body, nullptr);
    }

    // Add pseudo-headers if present
    if (!cached_stream->authority.empty()) {
        result->setKeyValue("authority", new QoreStringNode(cached_stream->authority), nullptr);
    }
    if (!cached_stream->scheme.empty()) {
        result->setKeyValue("scheme", new QoreStringNode(cached_stream->scheme), nullptr);
    }
    // RFC 8441: Add :protocol pseudo-header for extended CONNECT
    if (!cached_stream->connect_protocol.empty()) {
        headers->setKeyValue(":protocol", new QoreStringNode(cached_stream->connect_protocol), nullptr);
    }

    // Include trailers from client if present (e.g., gRPC client-streaming)
    if (!cached_stream->trailers.empty()) {
        QoreHashNode* trailers_hash = httpMultiHeadersToQoreHash(cached_stream->trailers, true);
        result->setKeyValue("trailers", trailers_hash, nullptr);
    }

    // Indicate headers-only dispatch (stream still in map for incremental reading)
    if (h2_state == H2S_HEADERS_READY) {
        result->setKeyValue("hdr", true, nullptr);
        // Indicate whether END_STREAM was on the HEADERS frame itself (no body expected)
        if (cached_stream->headers_end_stream) {
            result->setKeyValue("headers_end_stream", true, nullptr);
        }
    }

    return result;
}

Http2SessionPtr SocketHttp2ServerPollOperation::takeSession() {
    return nullptr;
}

SocketHttp2SendResponsePollOperation::SocketHttp2SendResponsePollOperation(ExceptionSink* xsink,
        QoreSocketObject* sock, const Http2SessionPtr& h2_session_param, int32_t stream_id, int status_code,
        const QoreHashNode* headers, const BinaryNode* body, bool is_connect, bool defer_init)
        : SocketPollSocketOperationBase(sock), h2_session(h2_session_param), stream_id(stream_id),
          status_code(status_code), is_connect(is_connect) {

    // Build headers as vector of pairs to support duplicate header names (e.g., set-cookie)
    if (headers) {
        ConstHashIterator hi(headers);
        while (hi.next()) {
            const char* key = hi.getKey();
            QoreValue val = hi.get();
            if (val.getType() == NT_STRING) {
                QoreStringValueHelper str(val);
                hdr_pairs.emplace_back(key, str->c_str());
            } else if (val.getType() == NT_LIST) {
                // Emit separate header entries for each list value
                const QoreListNode* l = val.get<const QoreListNode>();
                for (size_t i = 0; i < l->size(); ++i) {
                    QoreValue lv = l->retrieveEntry(i);
                    if (lv.getType() == NT_STRING) {
                        QoreStringValueHelper str(lv);
                        hdr_pairs.emplace_back(key, str->c_str());
                    }
                }
            }
        }
    }

    init(xsink, defer_init);
    if (*xsink) {
        return;
    }

    if (body && body->size()) {
        this->body = new BinaryNode;
        this->body->append(body->getPtr(), body->size());
    }
}

SocketHttp2SendResponsePollOperation::~SocketHttp2SendResponsePollOperation() {
}

void SocketHttp2SendResponsePollOperation::init(ExceptionSink* xsink, bool defer_init) {
    controller_deferred_init = defer_init;
    controller_deferred_tid = defer_init ? q_gettid() : -1;
    if (defer_init) {
        return;
    }

    AutoLocker al(sock->priv->m);
    initLocked(xsink);
}

int SocketHttp2SendResponsePollOperation::initLocked(ExceptionSink* xsink) {
    assert(sock->priv->m.trylock());
    if (initialized) {
        return 0;
    }
    initialized = true;

    Http2Session* session = sock->priv->socket->priv->h2_session.get();
    if (!session) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session available; startPollSendHttp2Response() must be "
            "called after startPollReadHttp2Request() completes on an active HTTP/2 connection");
        return -1;
    }
    h2_session = sock->priv->socket->priv->h2_session;

    if (sock->priv->checkOpen(xsink)) {
        return -1;
    }

    int rc = controller_deferred_init
        ? sock->priv->setNonBlockFromAsyncController(xsink, NB_ALL, controller_deferred_tid)
        : sock->priv->setNonBlock(xsink);
    if (rc) {
        return -1;
    }
    set_non_block = true;
    return 0;
}

QoreHashNode* SocketHttp2SendResponsePollOperation::continuePoll(ExceptionSink* xsink) {
    auto clear_non_block = [&]() {
        if (set_non_block) {
            AutoLocker al(sock->priv->m);
            if (set_non_block) {
                set_non_block = false;
                sock->priv->clearNonBlock();
            }
        }
    };

    if (!initialized) {
        AutoLocker al(sock->priv->m);
        if (initLocked(xsink)) {
            return nullptr;
        }
    }

    {
        AutoLocker al(sock->priv->m);
        if (!sock->priv->socket->priv->isOpen()) {
            xsink->raiseException("HTTP2-ERROR", "socket closed during poll operation");
            return nullptr;
        }
    }

    Http2Session* session = h2_session.get();
    if (!session) {
        xsink->raiseException("HTTP2-ERROR", "HTTP/2 session no longer available");
        return nullptr;
    }

    while (true) {
        switch (h2_state) {
            case H2S_NONE: {
                int rv;
                if (is_connect) {
                    // RFC 8441: CONNECT response (WebSocket over HTTP/2) - no body, no END_STREAM
                    // submitConnectResponse still uses map (CONNECT doesn't need duplicate headers)
                    strcase_str_map_t hdr_map;
                    for (const auto& p : hdr_pairs) {
                        hdr_map[p.first] = p.second;
                    }
                    if (getenv("QORE_HTTP2_DEBUG")) {
                        fprintf(stderr, "HTTP2 DEBUG: send CONNECT response stream=%d status=%d\n",
                            stream_id, status_code);
                        fflush(stderr);
                    }
                    rv = session->submitConnectResponse(stream_id, status_code, hdr_map, xsink);
                } else {
                    const void* body_ptr = body ? body->getPtr() : nullptr;
                    size_t body_len = body ? body->size() : 0;
                    printd(5, "SocketHttp2SendResponsePollOperation() submitting response stream_id=%d "
                        "status=%d body_len=%zu\n", stream_id, status_code, body_len);
                    rv = session->submitResponse(stream_id, status_code, hdr_pairs, body_ptr, body_len, xsink);
                }
                if (rv != 0 && !*xsink) {
                    xsink->raiseException("HTTP2-ERROR", "failed to submit HTTP/2 response: rv=%d", rv);
                }
                if (*xsink) {
                    return nullptr;
                }

                printd(5, "SocketHttp2SendResponsePollOperation() submitResponse succeeded, want_write=%d\n",
                    nghttp2_session_want_write(session->getSession()));
                h2_state = H2S_SENDING;
                continue;
            }

            case H2S_SENDING: {
                // Send pending data using proper async I/O (non-blocking)
                int rv = session->sendPendingData(0, xsink);
                if (*xsink) {
                    return nullptr;
                }
                // sendPendingData returns:
                //   0: success
                //   SOCK_POLLIN: need to poll for read (TLS renegotiation)
                //   SOCK_POLLOUT: need to poll for write
                //   -1: error (exception set)
                if (rv == SOCK_POLLIN || rv == SOCK_POLLOUT) {
                    // Need to poll for the direction SSL indicated
                    return getSocketPollInfoHash(xsink, rv);
                }

                // Check if there's still data in our buffer that wasn't sent yet
                if (session->hasPendingData()) {
                    // More data in buffer - poll for POLLOUT and retry
                    return getSocketPollInfoHash(xsink, SOCK_POLLOUT);
                }

                // Check if nghttp2 has more data to produce
                if (session->wantWrite()) {
                    continue;
                }

                // Data has been written to the socket buffer - transition to FLUSHING
                // to poll for POLLOUT one more time and verify all data is sent
                h2_state = H2S_FLUSHING;
                return getSocketPollInfoHash(xsink, SOCK_POLLOUT);
            }

            case H2S_FLUSHING: {
                // After POLLOUT, try to send any remaining buffered data (non-blocking)
                // Try to send any remaining data (non-blocking)
                int rv = session->sendPendingData(0, xsink);

                if (*xsink) {
                    return nullptr;
                }

                // sendPendingData returns:
                //   0: success
                //   SOCK_POLLIN: need to poll for read (TLS renegotiation)
                //   SOCK_POLLOUT: need to poll for write
                //   -1: error (exception set)
                if (rv == SOCK_POLLIN || rv == SOCK_POLLOUT) {
                    // SSL/socket would block - need to poll for the indicated direction
                    return getSocketPollInfoHash(xsink, rv);
                }

                // Check if there's still data in our send buffer that wasn't sent yet
                if (session->hasPendingData()) {
                    // More data in buffer - poll for POLLOUT and retry
                    return getSocketPollInfoHash(xsink, SOCK_POLLOUT);
                }

                if (session->wantWrite()) {
                    // nghttp2 has more data to produce (flow control, etc.)
                    h2_state = H2S_SENDING;
                    continue;
                }

                // All done - both nghttp2 and our buffer are empty
                h2_state = H2S_SENT;
                // Goal reached - clear non-block flag so subsequent operations can proceed
                clear_non_block();
                return nullptr;
            }

            case H2S_SENT:
                // Goal reached - clear non-block flag so subsequent operations can proceed
                clear_non_block();
                return nullptr;

            default:
                xsink->raiseException("HTTP2-ERROR", "unexpected state: %d", h2_state);
                return nullptr;
        }
    }
}

Http2SessionPtr SocketHttp2SendResponsePollOperation::takeSession() {
    // Session is now managed by socket, not this operation
    return nullptr;
}

SocketHttp2SendStreamingResponsePollOperation::SocketHttp2SendStreamingResponsePollOperation(
        ExceptionSink* xsink, QoreSocketObject* sock, int32_t stream_id, int status_code,
        const QoreHashNode* headers, InputStream* input_stream, QoreObject* input_stream_obj,
        int64 chunk_size)
        : SocketPollSocketOperationBase(sock), stream_id(stream_id),
          status_code(status_code),
          input_stream(input_stream), input_stream_obj(input_stream_obj),
          chunk_size(chunk_size > 0 ? chunk_size : 16384),
          is_pollable(false) {

    if (!input_stream->isIoThreadSafe()) {
        xsink->raiseException("HTTP-STREAM-ERROR", "InputStream is not I/O thread safe");
        return;
    }

    stream_fd = qore_input_stream_pollable_descriptor(input_stream, "HTTP-STREAM-ERROR", xsink);
    if (*xsink) {
        return;
    }
    is_pollable = stream_fd >= 0;

    AutoLocker al(sock->priv->m);

    // Get HTTP/2 session from socket
    Http2Session* session = sock->priv->socket->priv->h2_session.get();
    if (!session) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session available; "
            "startPollSendHttp2StreamingResponse() must be called after "
            "startPollReadHttp2Request() completes on an active HTTP/2 connection");
        return;
    }
    h2_session = sock->priv->socket->priv->h2_session;

    if (sock->priv->checkOpen(xsink)) {
        return;
    }

    if (sock->priv->setNonBlock(xsink)) {
        return;
    }
    set_non_block = true;

    // Build headers map; extract Content-Length for short-stream detection
    // so EOF before the declared length can be reported to the peer via
    // RST_STREAM (matching the H1 "server closes the connection" behavior).
    if (headers) {
        ConstHashIterator hi(headers);
        while (hi.next()) {
            const char* key = hi.getKey();
            QoreValue val = hi.get();
            if (val.getType() == NT_STRING) {
                QoreStringValueHelper str(val);
                const char* str_val = str->c_str();
                header_pairs.emplace_back(key, str_val);
                if (!strcasecmp(key, "content-length")) {
                    char* endptr = nullptr;
                    long long cl = strtoll(str_val, &endptr, 10);
                    if (endptr != str_val && cl >= 0) {
                        content_length = cl;
                    }
                }
            }
        }
    }

    // Thread affinity ownership chain for the InputStream:
    //   1. Handler thread creates the InputStream (owns thread affinity)
    //   2. QPP method startPollSendHttp2StreamingResponse() constructs this operation,
    //      then calls body->unassignThread() to release affinity from the handler thread
    //   3. On first continuePoll() call (from the I/O worker thread), need_reassign triggers
    //      input_stream->reassignThread() to claim affinity on the worker thread
    //   4. Headers are submitted, and all subsequent reads happen on that worker
    //      thread until completion.
}

SocketHttp2SendStreamingResponsePollOperation::~SocketHttp2SendStreamingResponsePollOperation() {
}

QoreHashNode* SocketHttp2SendStreamingResponsePollOperation::continuePoll(ExceptionSink* xsink) {
    // Reassign the input stream to the current (worker) thread on first call
    if (need_reassign) {
        need_reassign = false;
        if (input_stream) {
            input_stream->reassignThread(xsink);
            if (*xsink) return nullptr;
        }
    }

    auto clear_non_block = [&]() {
        if (set_non_block) {
            AutoLocker al(sock->priv->m);
            if (set_non_block) {
                set_non_block = false;
                sock->priv->clearNonBlock();
            }
        }
    };

    {
        AutoLocker al(sock->priv->m);
        if (!sock->priv->socket->priv->isOpen()) {
            xsink->raiseException("HTTP2-ERROR", "socket closed during poll operation");
            return nullptr;
        }
    }

    Http2Session* session = h2_session.get();
    if (!session) {
        xsink->raiseException("HTTP2-ERROR", "HTTP/2 session no longer available");
        return nullptr;
    }

    while (true) {
        switch (ss_state) {
            case SS_SUBMIT_HEADERS: {
                strcase_str_map_t hdr_map;
                for (const auto& p : header_pairs) {
                    hdr_map[p.first] = p.second;
                }

                // Submit streaming response (headers only, deferred data provider)
                int rv = session->submitResponseStreaming(stream_id, status_code, hdr_map, xsink);
                if (rv != 0 && !*xsink) {
                    xsink->raiseException("HTTP2-ERROR", "failed to submit HTTP/2 streaming response: rv=%d", rv);
                }
                if (*xsink) {
                    return nullptr;
                }

                printd(5, "SocketHttp2SendStreamingResponsePollOperation() headers submitted stream_id=%d\n",
                    stream_id);
                ss_state = SS_READ_CHUNK;
                continue;
            }

            case SS_READ_CHUNK: {
                if (eof) {
                    // If a Content-Length was declared and we've sent fewer
                    // bytes than promised, the InputStream ran out early.
                    // Send RST_STREAM with INTERNAL_ERROR so the client sees
                    // a concrete H2 stream error instead of an END_STREAM
                    // carrying truncated data (which would otherwise be
                    // indistinguishable from a valid short response and
                    // surface as FUTURE-TIMEOUT on the client when the H2
                    // layer keeps waiting for the declared byte count).
                    // Mirrors the H1 behavior at QoreSocket.cpp:7376-7382
                    // (HTTP-STREAM-ERROR + Phase::Error + connection close).
                    if (content_length >= 0 && bytes_sent < content_length) {
                        ExceptionSink rst_xsink;
                        sock->resetHttp2StreamAsync(stream_id, NGHTTP2_INTERNAL_ERROR, &rst_xsink);
                        rst_xsink.clear();
                        xsink->raiseException("HTTP-STREAM-ERROR",
                            "InputStream provided " QLLD " bytes but "
                            "Content-Length is " QLLD,
                            bytes_sent, content_length);
                        return nullptr;
                    }
                    // Send END_STREAM
                    int rv = session->sendStreamData(stream_id, nullptr, 0, true, xsink);
                    if (*xsink) return nullptr;
                    if (rv != 0) {
                        xsink->raiseException("HTTP2-ERROR", "failed to send END_STREAM");
                        return nullptr;
                    }
                    ss_state = SS_FLUSH;
                    continue;
                }

                if (is_pollable) {
                    // Non-blocking read for pollable streams.
                    // Probe readiness without blocking,
                    // then readNonBlock() to get the data. This distinguishes:
                    // - poll ready + readNonBlock returns 0 → true EOF
                    // - poll not ready → would block, yield to event loop for socket I/O
                    assert(stream_fd >= 0);
                    int poll_rv = qore_socket_poll_fd_now(stream_fd, POLLIN, "HTTP2-ERROR", "stream", xsink);
                    if (poll_rv < 0) {
                        // Send RST_STREAM to notify client of the error
                        ExceptionSink rst_xsink;
                        sock->resetHttp2StreamAsync(stream_id, NGHTTP2_INTERNAL_ERROR, &rst_xsink);
                        rst_xsink.clear();
                        return nullptr;
                    }
                    if (poll_rv == 0) {
                        // Stream not ready: register fd with event loop and
                        // retry reading on next continuePoll() call.
                        std::vector<std::pair<int, int>> extra_fds{{stream_fd, SOCK_POLLIN}};
                        return getSocketPollInfoHash(xsink, 0, extra_fds);
                    }

                    // Stream FD is readable — do non-blocking read
                    SimpleRefHolder<BinaryNode> chunk(new BinaryNode);
                    chunk->preallocate(chunk_size);
                    int64 count = input_stream->readNonBlock(
                        const_cast<void*>(chunk->getPtr()), chunk_size, xsink);
                    if (*xsink) {
                        // Send RST_STREAM to notify client of the error
                        ExceptionSink rst_xsink;
                        sock->resetHttp2StreamAsync(stream_id, NGHTTP2_INTERNAL_ERROR, &rst_xsink);
                        rst_xsink.clear();
                        return nullptr;
                    }
                    if (count == 0) {
                        // poll said readable but read returned 0 → EOF
                        eof = true;
                        continue;
                    }
                    chunk->setSize(count);
                    current_chunk = chunk.release();
                } else {
                    // Non-pollable (memory) streams - read() never blocks
                    current_chunk = input_stream->readHelper(chunk_size, xsink);
                    if (*xsink) {
                        // Send RST_STREAM to notify client of the error
                        ExceptionSink rst_xsink;
                        sock->resetHttp2StreamAsync(stream_id, NGHTTP2_INTERNAL_ERROR, &rst_xsink);
                        rst_xsink.clear();
                        return nullptr;
                    }
                    if (!current_chunk) {
                        eof = true;
                        continue;
                    }
                }

                printd(5, "SocketHttp2SendStreamingResponsePollOperation::continuePoll() "
                    "read chunk size=%zu\n", current_chunk->size());
                ss_state = SS_SEND_CHUNK;
                continue;
            }

            case SS_SEND_CHUNK: {
                size_t chunk_bytes = current_chunk->size();
                // Send the chunk as HTTP/2 DATA frames (not end_stream)
                int rv = session->sendStreamData(stream_id, current_chunk->getPtr(),
                    chunk_bytes, false, xsink);
                if (*xsink) return nullptr;
                if (rv != 0) {
                    xsink->raiseException("HTTP2-ERROR", "failed to send stream data");
                    return nullptr;
                }
                bytes_sent += (int64_t)chunk_bytes;
                current_chunk = nullptr;
                ss_state = SS_FLUSH;
                continue;
            }

            case SS_FLUSH: {
                // Flush pending data to socket
                int rv = session->sendPendingData(0, xsink);
                if (*xsink) return nullptr;

                if (rv == SOCK_POLLIN || rv == SOCK_POLLOUT) {
                    return getSocketPollInfoHash(xsink, rv);
                }

                if (session->hasPendingData()) {
                    return getSocketPollInfoHash(xsink, SOCK_POLLOUT);
                }

                if (session->wantWrite()) {
                    continue;
                }

                // Check if the flow control window is exhausted.  If so, we
                // need to receive a WINDOW_UPDATE from the client before we can
                // send more data or declare completion.
                //
                // getStreamRemoteWindowSize() returns -1 when the stream no
                // longer exists (already closed after END_STREAM was sent and
                // acknowledged).  A negative value does NOT mean the window is
                // exhausted — it means the stream is gone, so we must NOT
                // enter SS_RECV_WINDOW (which would loop forever waiting for a
                // WINDOW_UPDATE that will never arrive).
                {
                    int32_t stream_window = session->getStreamRemoteWindowSize(stream_id);
                    int32_t conn_window = session->getStreamRemoteWindowSize(0);
                    if ((stream_window >= 0 && stream_window == 0) || conn_window == 0) {
                        ss_state = SS_RECV_WINDOW;
                        return getSocketPollInfoHash(xsink, SOCK_POLLIN);
                    }
                }

                if (eof) {
                    // All done
                    ss_state = SS_DONE;
                    clear_non_block();
                    return nullptr;
                }

                // More data to read
                ss_state = SS_READ_CHUNK;
                continue;
            }

            case SS_RECV_WINDOW: {
                // Read incoming data from client (WINDOW_UPDATE, PING, etc.)
                // to update flow control windows so we can continue sending.
                int rv = session->receiveData(0, xsink);
                if (*xsink) return nullptr;
                if (rv == -1) {
                    // Would block - poll for read
                    return getSocketPollInfoHash(xsink, SOCK_POLLIN);
                }
                if (rv == 1) {
                    // Peer closed connection during streaming response
                    clear_non_block();
                    ss_state = SS_DONE;
                    return nullptr;
                }
                // Data received - WINDOW_UPDATE may have been processed.
                // Try to flush buffered data now that the window may be open.
                ss_state = SS_FLUSH;
                continue;
            }

            case SS_DONE:
                clear_non_block();
                return nullptr;

            default:
                xsink->raiseException("HTTP2-ERROR", "unexpected streaming state: %d", ss_state);
                return nullptr;
        }
    }
}

// HTTP/2 Flush Poll Operation implementation

SocketHttp2FlushPollOperation::SocketHttp2FlushPollOperation(ExceptionSink* xsink,
        QoreSocketObject* sock, bool defer_init, bool submit_ping, bool missing_h2_ok)
        : SocketPollSocketOperationBase(sock), submit_ping(submit_ping), missing_h2_ok(missing_h2_ok) {
    init(xsink, defer_init);
}

void SocketHttp2FlushPollOperation::init(ExceptionSink* xsink, bool defer_init) {
    controller_deferred_init = defer_init;
    controller_deferred_tid = defer_init ? q_gettid() : -1;
    if (defer_init) {
        return;
    }

    AutoLocker al(sock->priv->m);
    initLocked(xsink);
}

int SocketHttp2FlushPollOperation::initLocked(ExceptionSink* xsink) {
    assert(sock->priv->m.trylock());
    if (initialized) {
        return 0;
    }
    initialized = true;

    if (sock->priv->checkOpen(xsink)) {
        return -1;
    }
    h2_session = sock->priv->socket->priv->h2_session;
    if (!h2_session) {
        if (missing_h2_ok) {
            h2f_state = H2F_DONE;
            return 0;
        }
        xsink->raiseException("HTTP2-ERROR", "HTTP/2 session no longer available");
        return -1;
    }

    if (controller_deferred_init && (sock->priv->non_block_flags || sock->priv->non_block_accept_count > 0)) {
        // Nested flush while a legacy public poll operation owns non-blocking
        // mode.  The fd is already non-blocking; do not claim or clear the
        // parent's non-block flag.
        return sock->priv->checkAsyncSequenceAllowedForTid(xsink, NB_ALL, controller_deferred_tid);
    }

    int rc = controller_deferred_init
        ? sock->priv->setNonBlockFromAsyncController(xsink, NB_ALL, controller_deferred_tid)
        : sock->priv->setNonBlock(xsink);
    if (rc) {
        return -1;
    }
    set_non_block = true;
    return 0;
}

QoreHashNode* SocketHttp2FlushPollOperation::continuePoll(ExceptionSink* xsink) {
    auto clear_non_block = [&]() {
        if (set_non_block) {
            AutoLocker al(sock->priv->m);
            if (set_non_block) {
                set_non_block = false;
                sock->priv->clearNonBlock();
            }
        }
    };

    if (!initialized) {
        AutoLocker al(sock->priv->m);
        if (initLocked(xsink)) {
            return nullptr;
        }
    }
    if (h2f_state == H2F_DONE) {
        return nullptr;
    }

    {
        AutoLocker al(sock->priv->m);
        if (!sock->priv->socket->priv->isOpen()) {
            xsink->raiseException("HTTP2-ERROR", "socket closed during poll operation");
            return nullptr;
        }
    }

    Http2Session* session = h2_session.get();
    if (!session) {
        xsink->raiseException("HTTP2-ERROR", "HTTP/2 session no longer available");
        return nullptr;
    }

    if (submit_ping && !ping_submitted) {
        int rv = session->submitPing(nullptr, xsink);
        if (rv < 0 || *xsink) {
            return nullptr;
        }
        ping_submitted = true;
    }

    while (true) {
        switch (h2f_state) {
            case H2F_FLUSHING: {
                int rv = session->sendPendingData(0, xsink);
                if (*xsink) {
                    return nullptr;
                }
                if (rv < 0) {
                    return nullptr;
                }
                if (rv == SOCK_POLLIN || rv == SOCK_POLLOUT) {
                    return getSocketPollInfoHash(xsink, rv);
                }

                if (session->hasPendingData()) {
                    return getSocketPollInfoHash(xsink, SOCK_POLLOUT);
                }

                if (session->wantWrite()) {
                    continue;
                }

                // @c session_want_write returns 0 AND our send_buffer is
                // empty — but a streaming response's data provider may still
                // have bytes queued that nghttp2 hasn't generated a DATA frame
                // for because the connection-level flow-control window is
                // exhausted.  In that case the only way forward is to drain
                // the read side, so peer WINDOW_UPDATE frames re-open the
                // window and let the next @c session_mem_send pull from the
                // data provider.
                //
                // This is the real-world case exposed by
                // Http2.qtest::testHttp2RstStreamDuringInputStreamStreaming:
                // peer RSTs a streaming response mid-flight, consuming 65535
                // bytes of connection window.  A fresh response on another
                // stream can send HEADERS but stays flow-control-blocked on
                // DATA until the peer credits the window back.  Without this
                // read-drain the caller deadlocks on flush.
                if (session->hasUnsentStreamData()) {
                    int recv_rv = session->receiveData(0, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (recv_rv == 1) {
                        // Peer closed — we can't complete the flush, but
                        // terminate cleanly instead of spinning.
                        h2f_state = H2F_DONE;
                        clear_non_block();
                        return nullptr;
                    }
                    if (recv_rv == -1) {
                        // Would block: wait for either readable (peer's
                        // WINDOW_UPDATE) or writable (our re-generated DATA
                        // frame once the window reopens).
                        return getSocketPollInfoHash(xsink,
                            SOCK_POLLIN | SOCK_POLLOUT);
                    }
                    // Processed incoming data (WINDOW_UPDATE, trailers, etc.);
                    // loop back to re-attempt sendPendingData.
                    continue;
                }

                // All pending data flushed and no streams have queued data.
                h2f_state = H2F_DONE;
                clear_non_block();
                return nullptr;
            }

            default:
                xsink->raiseException("HTTP2-ERROR", "unexpected flush state: %d", h2f_state);
                return nullptr;
        }
    }
}

// HTTP/2 Client Multiplex Poll Operation implementation

SocketHttp2ClientMultiplexPollOperation::SocketHttp2ClientMultiplexPollOperation(ExceptionSink* xsink,
        QoreSocketObject* sock)
        : SocketPollSocketOperationBase(sock) {
    AutoLocker al(sock->priv->m);

    // Check if socket is open and valid
    if (sock->priv->checkOpen(xsink)) {
        return;
    }

    // Set non-blocking mode
    if (sock->priv->setNonBlock(xsink)) {
        return;
    }
    set_non_block = true;

    // Check if there's already an HTTP/2 session stored in the socket (from a previous request)
    bool reused_session = false;
    if (sock->priv->socket->priv->h2_session) {
        // Reuse existing session (socket owns it)
        h2_session = sock->priv->socket->priv->h2_session;
        // Update max body size limit in case it changed since session was created
        h2_session->setMaxRequestBodySize(
            sock->priv->socket->priv->max_http2_body_size.load(std::memory_order_relaxed));
        reused_session = true;
    } else {
        // Initialize new HTTP/2 client session and store it on the socket
        if (initSession(xsink)) {
            sock->priv->clearNonBlock();
            set_non_block = false;
            return;
        }
    }

    // Set up stream completion callback to queue responses
    // Capture callback_guard by value (shared_ptr copy) to ensure it outlives this object.
    // The mutex in the guard ensures no TOCTOU race between checking the destroyed flag
    // and using 'this' - both happen while holding the mutex.
    auto guard = callback_guard;
    h2_session->setStreamCompleteCallback(
        [this, guard](int32_t stream_id, Http2StreamInfo* stream, ExceptionSink* xsink) {
            // Lock mutex and check if poll operation is being destroyed
            // The lock ensures we don't race with the destructor
            std::lock_guard<std::mutex> lg(guard->mutex);
            if (guard->destroyed) {
                printd(5, "onStreamComplete: poll operation destroyed, ignoring callback\n");
                return;
            }
            this->onStreamComplete(stream_id, stream, xsink);
        });

    // Set initial state based on whether we reused a session
    if (reused_session) {
        // Session already established - go directly to reading frames
        h2_state = H2C_READING;
    } else {
        // New session - need to exchange connection preface
        h2_state = H2C_SEND_PREFACE;
    }
}

SocketHttp2ClientMultiplexPollOperation::~SocketHttp2ClientMultiplexPollOperation() {
    printd(5, "~SocketHttp2ClientMultiplexPollOperation() starting\n");

    // Set destroyed flag while holding the mutex.
    // This ensures any in-flight callback completes before we proceed, and any
    // callback that starts after this will see destroyed=true and return early.
    // The mutex eliminates the TOCTOU race between flag check and 'this' usage.
    {
        std::lock_guard<std::mutex> lg(callback_guard->mutex);
        callback_guard->destroyed = true;
    }

    // Clear stream completion callback - session is guaranteed valid via shared_ptr
    if (h2_session) {
        printd(5, "~SocketHttp2ClientMultiplexPollOperation() clearing callback\n");
        h2_session->clearStreamCompleteCallback();
        printd(5, "~SocketHttp2ClientMultiplexPollOperation() callback cleared\n");
    }

    // Deref any queued responses
    printd(5, "~SocketHttp2ClientMultiplexPollOperation() getting lock for responses\n");
    if (getenv("QORE_HTTP2_DEBUG")) {
        fprintf(stderr, "HTTP2 DEBUG: ~SocketHttp2ClientMultiplexPollOperation() getting lock for responses\n");
        fflush(stderr);
    }
    AutoLocker al(response_lock);
    printd(5, "~SocketHttp2ClientMultiplexPollOperation() derefing %zu responses\n", completed_responses.size());
    if (getenv("QORE_HTTP2_DEBUG")) {
        fprintf(stderr, "HTTP2 DEBUG: ~SocketHttp2ClientMultiplexPollOperation() derefing %zu responses\n",
            completed_responses.size());
        fflush(stderr);
    }
    for (QoreHashNode* h : completed_responses) {
        h->deref(nullptr);
    }
    completed_responses.clear();
    printd(5, "~SocketHttp2ClientMultiplexPollOperation() done\n");
}

int SocketHttp2ClientMultiplexPollOperation::initSession(ExceptionSink* xsink) {
    // Determine scheme from socket state (secure = https, otherwise http)
    const char* scheme = sock->priv->socket->priv->ssl ? "https" : "http";

    // Create client-side HTTP/2 session using the underlying QoreSocket's priv
    h2_session = Http2Session::createClient(sock->priv->socket->priv, xsink, scheme);
    if (!h2_session) {
        return -1;
    }
    // Propagate max request body size limit to the HTTP/2 session
    int64 max_http2_body_size = sock->priv->socket->priv->max_http2_body_size.load(std::memory_order_relaxed);
    if (max_http2_body_size > 0) {
        h2_session->setMaxRequestBodySize(max_http2_body_size);
    }
    // Store session on socket for shared access
    sock->priv->socket->priv->h2_session = h2_session;
    return 0;
}

void SocketHttp2ClientMultiplexPollOperation::onStreamComplete(int32_t stream_id, Http2StreamInfo* stream,
        ExceptionSink* xsink) {
    // Build response hash from stream info
    ReferenceHolder<QoreHashNode> response(new QoreHashNode(autoTypeInfo), xsink);

    // If the stream was reset by the peer (RST_STREAM with non-zero error
    // code), surface it as an err/desc pair so the conn_mgr response path
    // (send_internal_conn_mgr) raises a proper exception instead of letting
    // the partial body bubble up as a successful short response (observed as
    // FUTURE-TIMEOUT with a Content-Length mismatch, because the H1 body
    // reader would otherwise spin waiting for the declared byte count).
    if (stream->reset && stream->error_code != 0) {
        char errbuf[64];
        snprintf(errbuf, sizeof(errbuf), "HTTP/2 stream %d reset by peer "
            "(error_code=%u)", stream_id, stream->error_code);
        response->setKeyValue("err", new QoreStringNode("HTTP2-STREAM-RESET"),
            xsink);
        response->setKeyValue("desc", new QoreStringNode(errbuf), xsink);
        response->setKeyValue("stream_id", stream_id, xsink);
        // Also mark the stream as ended so the waiter stops polling.
        response->setKeyValue("end_stream", true, xsink);
        {
            AutoLocker al(response_lock);
            completed_responses.push_back(response.release());
        }
        return;
    }

    response->setKeyValue("stream_id", stream_id, xsink);
    response->setKeyValue("status_code", stream->status_code, xsink);

    // Convert headers map to Qore hash (handle duplicate headers per RFC 7540)
    if (!stream->headers.empty()) {
        response->setKeyValue("headers", httpMultiHeadersToQoreHash(stream->headers), xsink);
    }

    // Convert body vector to Qore binary or string based on content-type
    if (!stream->body.empty()) {
        // Check if content-type indicates text-based content
        bool is_text = false;
        bool force_utf8 = false;
        std::string media_type;
        auto ct_it = stream->headers.find("content-type");
        if (ct_it != stream->headers.end() && !ct_it->second.empty()) {
            // Extract media type (before any parameters like charset)
            media_type = ct_it->second.back();
            size_t semicolon = media_type.find(';');
            if (semicolon != std::string::npos) {
                media_type = media_type.substr(0, semicolon);
            }
            // Trim whitespace
            while (!media_type.empty() && isspace(static_cast<unsigned char>(media_type.back()))) {
                media_type.pop_back();
            }
            while (!media_type.empty() && isspace(static_cast<unsigned char>(media_type.front()))) {
                media_type.erase(0, 1);
            }
            // Convert to lowercase for comparison
            std::transform(media_type.begin(), media_type.end(), media_type.begin(),
                [](unsigned char c) { return std::tolower(c); });

            // Check for text types
            is_text = (media_type.size() >= 5 && media_type.compare(0, 5, "text/") == 0)
                || media_type == "application/json"
                || media_type == "application/xml"
                || media_type == "application/javascript"
                || media_type == "application/x-www-form-urlencoded"
                || (media_type.size() > 5 && media_type.compare(media_type.size() - 5, 5, "+json") == 0)
                || (media_type.size() > 4 && media_type.compare(media_type.size() - 4, 4, "+xml") == 0);

            // JSON and YAML are always UTF-8 per RFC 8259 / YAML spec
            force_utf8 = media_type == "application/json"
                || (media_type.size() > 5 && media_type.compare(media_type.size() - 5, 5, "+json") == 0)
                || media_type == "application/x-yaml" || media_type == "text/yaml"
                || media_type == "text/x-yaml" || media_type == "application/yaml";
        }

        // Skip text conversion if content-encoding indicates compression
        // (gzip, deflate, br, zstd, etc.) — the compressed bytes must stay
        // as binary until the upstream decompression layer in
        // send_internal_conn_mgr / process_binary_body runs.  Without this
        // check, compressed bytes are interpreted as UTF-8 and corrupted.
        bool has_content_encoding = false;
        {
            auto ce_it = stream->headers.find("content-encoding");
            if (ce_it != stream->headers.end() && !ce_it->second.empty()) {
                const std::string& ce = ce_it->second.back();
                // "identity" means no encoding — treat as uncompressed
                if (!ce.empty() && strcasecmp(ce.c_str(), "identity") != 0) {
                    has_content_encoding = true;
                }
            }
        }

        if (is_text && !has_content_encoding) {
            const QoreEncoding* enc = QCS_UTF8;
            if (!force_utf8) {
                // Extract charset from full content-type header (case-insensitive search)
                std::string full_ct_lower = ct_it->second.back();
                std::transform(full_ct_lower.begin(), full_ct_lower.end(),
                    full_ct_lower.begin(),
                    [](unsigned char c) { return std::tolower(c); });
                size_t charset_pos = full_ct_lower.find("charset=");
                if (charset_pos != std::string::npos) {
                    // Extract value from original (non-lowercased) string
                    std::string charset_val = ct_it->second.back().substr(charset_pos + 8);
                    // Trim trailing whitespace, semicolons, quotes
                    while (!charset_val.empty()
                            && (isspace(static_cast<unsigned char>(charset_val.back()))
                                || charset_val.back() == ';'
                                || charset_val.back() == '"')) {
                        charset_val.pop_back();
                    }
                    // Trim leading quotes
                    if (!charset_val.empty() && charset_val.front() == '"') {
                        charset_val.erase(0, 1);
                    }
                    if (!charset_val.empty()) {
                        const QoreEncoding* found = QEM.findCreate(charset_val.c_str());
                        if (found) {
                            enc = found;
                        }
                    }
                }
            }
            QoreStringNode* body_str = new QoreStringNode(
                reinterpret_cast<const char*>(stream->body.data()),
                stream->body.size(), enc);
            response->setKeyValue("body", body_str, xsink);
        } else {
            SimpleRefHolder<BinaryNode> body(new BinaryNode);
            body->append(stream->body.data(), stream->body.size());
            response->setKeyValue("body", body.release(), xsink);
        }
    }

    // Include trailers if present
    if (!stream->trailers.empty()) {
        response->setKeyValue("trailers", httpMultiHeadersToQoreHash(stream->trailers, true), xsink);
    }

    // Mark that the stream has ended (END_STREAM was received)
    response->setKeyValue("end_stream", true, xsink);

    // Queue the response
    {
        AutoLocker al(response_lock);
        completed_responses.push_back(response.release());
    }
}

QoreHashNode* SocketHttp2ClientMultiplexPollOperation::continuePoll(ExceptionSink* xsink) {
    ASYNC_IO_TRACE("H2Mux::continuePoll() acquiring lock...\n");
    auto clear_non_block = [&]() {
        if (set_non_block) {
            AutoLocker al(sock->priv->m);
            if (set_non_block) {
                set_non_block = false;
                sock->priv->clearNonBlock();
            }
        }
    };

    // Check if the socket was closed by another thread (e.g., connection
    // close during h2c probe timeout).  Without this check, subsequent
    // operations (brecv, send) would hit an assertion failure on the
    // invalid socket fd.
    {
        AutoLocker al(sock->priv->m);
        ASYNC_IO_TRACE("H2Mux::continuePoll() lock acquired\n");
        if (!sock->priv->socket->priv->isOpen() || !sock->priv->socket->priv->h2_session) {
            xsink->raiseException("HTTP2-ERROR", "socket closed during poll operation");
            return nullptr;
        }
    }

    while (true) {
        switch (h2_state) {
            case H2C_SEND_PREFACE: {
                // Send client connection preface (SETTINGS frame)
                int rv = h2_session->sendConnectionPrefaceNonBlocking(xsink);
                if (*xsink) {
                    return nullptr;
                }
                if (rv < 0) {
                    return nullptr;
                }
                if (rv == SOCK_POLLIN || rv == SOCK_POLLOUT) {
                    return getSocketPollInfoHash(xsink, rv);
                }
                h2_state = H2C_RECV_PREFACE;
                // Fall through to receive preface
            }

            case H2C_RECV_PREFACE:
            case H2C_READING: {
                // Check if there are completed streams - dispatch via callback
                if (h2_session->hasCompletedStreams()) {
                    // Flush any pending output before dispatching
                    int srv = h2_session->sendPendingData(0, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (srv == SOCK_POLLIN || srv == SOCK_POLLOUT) {
                        // Socket buffer full; poll and retry
                        return getSocketPollInfoHash(xsink, srv);
                    }
                    // Callbacks are invoked by the session via StreamCompleteCallback
                    // Continue reading for more responses
                }

                // First try to send any pending data (requests submitted from other threads
                // or left in send_buffer from a previous non-blocking send attempt)
                if (h2_session->wantWrite() || h2_session->hasPendingData()) {
                    int srv = h2_session->sendPendingData(0, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (srv == SOCK_POLLOUT) {
                        // Need to poll for write
                        return getSocketPollInfoHash(xsink, SOCK_POLLIN | SOCK_POLLOUT);
                    }
                }

                // Receive and process HTTP/2 frames
                int rv = h2_session->receiveData(0, xsink);
                if (*xsink) {
                    return nullptr;
                }
                if (rv == 1) {
                    // Connection closed by peer
                    peer_closed = true;
                    h2_state = H2C_CLOSED;
                    clear_non_block();
                    return nullptr;
                }
                if (rv == -1) {
                    // Would block - poll for read (and write if we have pending data)
                    int events = SOCK_POLLIN;
                    if (h2_session->wantWrite() || h2_session->hasPendingData()) {
                        events |= SOCK_POLLOUT;
                    }
                    return getSocketPollInfoHash(xsink, events);
                }

                // Data received - check for completed streams
                if (h2_session->hasCompletedStreams()) {
                    // Responses are dispatched via callback mechanism
                    // Continue the loop to process more frames
                    continue;
                }

                // Check for CONNECT streams that received response headers
                // but no END_STREAM (RFC 8441: server accepts the tunnel with
                // 200 OK, then stream stays open for bidirectional data).
                // Deliver headers as output so the caller knows the CONNECT
                // was accepted — without this, streaming CONNECT responses
                // would never be visible to getOutput().
                // NOTE: only check if there are actually CONNECT streams
                // pending — takeHeadersReadyStreamCopy() marks the stream as
                // dispatched, which would break normal (non-CONNECT) response
                // processing if called unconditionally.
                if (h2_session->hasHeadersReadyConnectStream()) {
                    std::unique_ptr<Http2StreamInfo> hdr_stream =
                        h2_session->takeHeadersReadyStreamCopy();
                    if (hdr_stream && hdr_stream->is_connect) {
                        ReferenceHolder<QoreHashNode> resp(new QoreHashNode(autoTypeInfo), xsink);
                        resp->setKeyValue("stream_id", (int64)hdr_stream->stream_id, xsink);
                        resp->setKeyValue("status_code", (int64)hdr_stream->status_code, xsink);
                        // Copy response headers
                        if (!hdr_stream->headers.empty()) {
                            ReferenceHolder<QoreHashNode> hdrs(new QoreHashNode(autoTypeInfo), xsink);
                            for (auto& [k, vals] : hdr_stream->headers) {
                                if (!vals.empty()) {
                                    hdrs->setKeyValue(k.c_str(),
                                        new QoreStringNode(vals[0]), xsink);
                                }
                            }
                            resp->setKeyValue("headers", hdrs.release(), xsink);
                        }
                        // NOT end_stream — the CONNECT tunnel is still open
                        // Mark the stream as streaming for subsequent data delivery
                        h2_session->setStreamStreaming(hdr_stream->stream_id);
                        {
                            AutoLocker rl(response_lock);
                            completed_responses.push_back(resp.release());
                        }
                        continue;
                    }
                }

                // Non-CONNECT streaming streams need a headers-only event dispatched
                // BEFORE the first intermediate body fragment, so downstream consumers
                // (ds_get_recv, send_internal_conn_mgr's per-chunk recv_callback, SSE
                // parsers) see status_code + Content-Type before any body bytes.  The
                // CONNECT handshake path above handles CONNECT streams via
                // takeHeadersReadyStreamCopy (which flips `dispatched`); non-CONNECT
                // streaming streams use a separate flag (`headers_streamed`) so
                // markStreamComplete still fires on END_STREAM to deliver the
                // terminal event with trailers.
                //
                // Without this split, small responses where the server ships HEADERS
                // in one frame and DATA+END_STREAM in a later frame leak the first
                // DATA chunk into recv_callback before the header callback has
                // recorded content-type — ds_get_recv then throws
                // DESERIALIZATION-ERROR with ct=null (see Qorus issue-1704.qtest).
                while (std::unique_ptr<Http2StreamInfo> hdr_stream =
                        h2_session->takeStreamingHeadersReadyCopy()) {
                    ReferenceHolder<QoreHashNode> resp(new QoreHashNode(autoTypeInfo), xsink);
                    resp->setKeyValue("stream_id", (int64)hdr_stream->stream_id, xsink);
                    resp->setKeyValue("status_code", (int64)hdr_stream->status_code, xsink);
                    if (!hdr_stream->headers.empty()) {
                        resp->setKeyValue("headers",
                            httpMultiHeadersToQoreHash(hdr_stream->headers), xsink);
                    }
                    // Intentionally NOT setting end_stream — the stream is still open,
                    // body chunks and end marker will follow via subsequent events.
                    {
                        AutoLocker rl(response_lock);
                        completed_responses.push_back(resp.release());
                    }
                }

                // Check for intermediate body data on streaming streams.
                // Batch limit prevents monopolizing the poll loop when many streams
                // have data; remaining data will be picked up on the next poll cycle.
                {
                    int32_t streaming_id = 0;
                    int batch = 0;
                    static const int MAX_STREAMING_BATCH = 16;
                    while (batch < MAX_STREAMING_BATCH
                            && h2_session->hasStreamingData(streaming_id)) {
                        // Take body data from the streaming stream
                        BinaryNode* body = h2_session->takeStreamData(streaming_id, 0, xsink);
                        if (*xsink) {
                            return nullptr;
                        }
                        if (!body) {
                            break;
                        }
                        // Create a partial response with just the body
                        ReferenceHolder<QoreHashNode> partial(new QoreHashNode(autoTypeInfo), xsink);
                        partial->setKeyValue("stream_id", streaming_id, xsink);
                        if (*xsink) {
                            return nullptr;
                        }
                        partial->setKeyValue("body", body, xsink);
                        if (*xsink) {
                            return nullptr;
                        }
                        // No end_stream flag - this is intermediate data
                        {
                            AutoLocker rl(response_lock);
                            completed_responses.push_back(partial.release());
                        }
                        ++batch;
                    }
                }

                // No completed streams yet - check if we're past the preface stage
                if (h2_state == H2C_RECV_PREFACE) {
                    h2_state = H2C_READING;
                }

                // Check for GOAWAY
                if (h2_session->isGoawayReceived()) {
                    h2_state = H2C_CLOSED;
                    clear_non_block();
                    return nullptr;
                }

                // Poll for more data (and write if we have pending data)
                int events = SOCK_POLLIN;
                if (h2_session->wantWrite() || h2_session->hasPendingData()) {
                    events |= SOCK_POLLOUT;
                }
                return getSocketPollInfoHash(xsink, events);
            }

            case H2C_CLOSED:
                clear_non_block();
                return nullptr;

            default:
                xsink->raiseException("HTTP2-ERROR", "unexpected client multiplex state: %d", h2_state);
                return nullptr;
        }
    }
}

SocketRecvFromPollOperation::SocketRecvFromPollOperation(ExceptionSink* xsink, size_t max_size,
        QoreSocketObject* sock) : SocketPollSocketOperationBase(sock), max_size(max_size), output(xsink) {
    AutoLocker al(sock->priv->m);

    // throw an exception and exit if the object is no longer open or valid
    if (sock->priv->checkOpen(xsink)) {
        return;
    }

    if (!sock->priv->setNonBlock(xsink)) {
        poll_state.reset(sock->priv->socket->startRecvFrom(xsink, max_size));
        if (!poll_state) {
            sock->priv->clearNonBlock();
        } else {
            set_non_block = true;
        }
    }
}

QoreHashNode* SocketRecvFromPollOperation::continuePoll(ExceptionSink* xsink) {
    AutoLocker al(sock->priv->m);

    // throw an exception and exit if the object is no longer open or valid
    if (sock->priv->checkOpen(xsink)) {
        return nullptr;
    }

    if (!poll_state) {
        return nullptr;
    }

    // see if we are able to continue
    int rc = poll_state->continuePoll(xsink);
    if (*xsink || !rc) {
        if (!*xsink) {
            // get output (hash with data + address info)
            output = poll_state->takeOutput().get<QoreHashNode>();
            received = true;
        }
        // release the AbstractPollState value
        poll_state.reset();
        sock->priv->clearNonBlock();
        set_non_block = false;
        return nullptr;
    }
    return getSocketPollInfoHash(xsink, rc);
}

SocketSendToPollOperation::SocketSendToPollOperation(ExceptionSink* xsink, const char* host, int port, int family,
        BinaryNode* data, QoreSocketObject* sock)
        : SocketPollSocketOperationBase(sock), host(host), family(q_get_af(family)), data(data) {
    AutoLocker al(sock->priv->m);

    // throw an exception and exit if the object is no longer open or valid
    if (sock->priv->checkOpen(xsink)) {
        data->deref();
        this->data = nullptr;
        return;
    }
    if (qore_socket_private::get(*sock->priv->socket)->stype != SOCK_DGRAM) {
        xsink->raiseException("SOCKET-SENDTO-ERROR", "sendto() is only valid for UDP (SOCK_DGRAM) sockets");
        data->deref();
        this->data = nullptr;
        return;
    }

    service = std::to_string(port);
}

SocketSendToPollOperation::~SocketSendToPollOperation() = default;

void SocketSendToPollOperation::cleanup(ExceptionSink* xsink) {
    if (data) {
        data->deref();
        data = nullptr;
    }
    resolver.reset();
    if (set_non_block) {
        sock->clearNonBlock();
        set_non_block = false;
    }
    poll_state.reset();
    sock->deref(xsink);
}

void SocketSendToPollOperation::abort(ExceptionSink* xsink) {
    if (data) {
        data->deref();
        data = nullptr;
    }
    resolver.reset();
    SocketPollSocketOperationBase::abort(xsink);
}

QoreHashNode* SocketSendToPollOperation::getResolverPollInfo(ExceptionSink* xsink) const {
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

int SocketSendToPollOperation::startSendToPollState(ExceptionSink* xsink) {
    assert(data);
    const std::vector<SocketResolvedAddrInfo>& addrs = resolver->getAddresses();
    if (addrs.empty()) {
        xsink->raiseException("SOCKET-SENDTO-ERROR", "could not resolve destination address '%s:%s'",
            host.c_str(), service.c_str());
        return -1;
    }

    const SocketResolvedAddrInfo& ai = addrs.front();
    memcpy(&dest_addr, &ai.addr, ai.addrlen);
    dest_addr_len = ai.addrlen;
    resolver.reset();

    AutoLocker al(sock->priv->m);
    if (sock->priv->checkOpen(xsink)) {
        return -1;
    }
    if (sock->priv->setNonBlock(xsink)) {
        return -1;
    }

    // startSendTo takes ownership of the BinaryNode reference if it returns a state.
    AbstractPollState* ps = sock->priv->socket->startSendTo(xsink, data,
        reinterpret_cast<const struct sockaddr*>(&dest_addr), dest_addr_len);
    if (!ps) {
        sock->priv->clearNonBlock();
        return -1;
    }

    poll_state.reset(ps);
    data = nullptr;
    if (*xsink) {
        poll_state.reset();
        sock->priv->clearNonBlock();
        return -1;
    }

    set_non_block = true;
    return 0;
}

QoreHashNode* SocketSendToPollOperation::continuePoll(ExceptionSink* xsink) {
    if (sent) {
        return nullptr;
    }

    if (!poll_state) {
        if (!resolver) {
            resolver = std::make_unique<QoreCaresAddrInfoResolver>(host, service, family, SOCK_DGRAM, 0);
        }

        int rc = resolver->continuePoll(xsink);
        if (*xsink || rc < 0) {
            return nullptr;
        }
        if (rc) {
            return getResolverPollInfo(xsink);
        }
        if (startSendToPollState(xsink)) {
            if (data) {
                data->deref();
                data = nullptr;
            }
            return nullptr;
        }
    }

    {
        AutoLocker al(sock->priv->m);

        // throw an exception and exit if the object is no longer open or valid
        if (sock->priv->checkOpen(xsink)) {
            return nullptr;
        }

        if (!poll_state) {
            return nullptr;
        }

        // see if we are able to continue
        int rc = poll_state->continuePoll(xsink);
        if (*xsink || !rc) {
            // release the AbstractPollState value
            poll_state.reset();
            sock->priv->clearNonBlock();
            set_non_block = false;
            if (!*xsink) {
                sent = true;
            }
            return nullptr;
        }
        return getSocketPollInfoHash(xsink, rc);
    }
    return nullptr;
}

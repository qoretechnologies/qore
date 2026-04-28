/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreFtpClient.cpp

    thread-safe QoreFtpClient object

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
#include <qore/QoreFtpClient.h>
#include <qore/QoreURL.h>
#include <qore/QoreSocket.h>

#include "qore/intern/QC_FtpClient.h"
#include "qore/intern/QC_FtpControlPollOperation.h"
#include "qore/intern/QC_FtpDataPollOperation.h"
#include "qore/intern/QC_Queue.h"
#include "qore/intern/QC_Socket.h"
#include "qore/intern/SocketSyncPoll.h"
#include "qore/intern/qore_socket_private.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "qore/intern/AsyncIoControllerPriv.h"

#define FTPDEBUG 5

//! to set the FTP mode
enum qore_ftp_mode {
    FTP_MODE_UNKNOWN,
    FTP_MODE_PORT,
    FTP_MODE_PASV,
    FTP_MODE_EPSV
    //FTP_MODE_LPSV
};

static void qore_ftp_raise_poll_result_exception(const QoreHashNode* ex, ExceptionSink* xsink) {
    QoreValue err = ex->getKeyValue("err");
    QoreValue desc = ex->getKeyValue("desc");
    QoreValue arg = ex->getKeyValue("arg");
    xsink->raiseException(
        err.getType() == NT_STRING
            ? err.get<const QoreStringNode>()->stringRefSelf()
            : new QoreStringNode("FTP-ASYNC-IO-ERROR"),
        desc.getType() == NT_STRING
            ? desc.get<const QoreStringNode>()->stringRefSelf()
            : new QoreStringNode("async FTP socket operation failed"),
        arg.refSelf());
}

static int qore_ftp_check_controller_result(const QoreHashNode* result, const char* what, ExceptionSink* xsink) {
    if (!result) {
        xsink->raiseException("FTP-ASYNC-IO-ERROR", "%s did not receive an async I/O result", what);
        return -1;
    }

    QoreValue ex = result->getKeyValue("ex");
    if (ex.getType() == NT_HASH) {
        qore_ftp_raise_poll_result_exception(ex.get<const QoreHashNode>(), xsink);
        return -1;
    }

    QoreValue canceled = result->getKeyValue("canceled");
    if (canceled.getAsBool()) {
        xsink->raiseException("FTP-ASYNC-IO-ERROR", "%s was canceled", what);
        return -1;
    }
    return 0;
}

static int qore_ftp_wait_controller_queue(QoreObject* queue_obj, const char* what, ExceptionSink* xsink) {
    if (!queue_obj) {
        xsink->raiseException("FTP-ASYNC-IO-ERROR", "%s did not receive an async I/O result queue", what);
        return -1;
    }

    ReferenceHolder<Queue> queue(
        static_cast<Queue*>(queue_obj->getReferencedPrivateData(CID_QUEUE, xsink)), xsink);
    if (*xsink) {
        return -1;
    }
    if (!queue) {
        xsink->raiseException("FTP-ASYNC-IO-ERROR", "%s received an invalid async I/O result queue", what);
        return -1;
    }

    bool timed_out = false;
    ValueHolder result(queue->shift(xsink, 0, &timed_out), xsink);
    if (*xsink) {
        return -1;
    }
    if (timed_out) {
        xsink->raiseException("SOCKET-TIMEOUT", "%s timed out waiting for async I/O completion", what);
        return -1;
    }
    if (result->getType() != NT_HASH) {
        xsink->raiseException("FTP-ASYNC-IO-ERROR", "%s expected SocketPollResultInfo from async I/O, got '%s'",
            what, result->getFullTypeName());
        return -1;
    }
    return qore_ftp_check_controller_result(result->get<const QoreHashNode>(), what, xsink);
}

class TmpLocalName {
public:
    DLLLOCAL TmpLocalName(const char* name1, const char* name2) : str(name1) {
        if (!name1) {
            tmp_str = q_basename(name2);
        }
    }

    DLLLOCAL ~TmpLocalName() {
        if (tmp_str) {
            free(tmp_str);
        }
    }

    DLLLOCAL const char* operator*() const {
        return str ? str : tmp_str;
    }

    DLLLOCAL void discard() {
        if (tmp_str) {
            free(tmp_str);
            tmp_str = nullptr;
        }
    }

private:
    const char* str;
    char* tmp_str = nullptr;
};

class FtpResp {
public:
    DLLLOCAL FtpResp() {}

    DLLLOCAL FtpResp(QoreStringNode* s) : str(s) {
    }

    DLLLOCAL ~FtpResp() {
        if (str) {
            str->deref();
        }
    }

    DLLLOCAL QoreStringNode* assign(QoreStringNode* s) {
        if (str) {
            str->deref();
        }
        str = s;
        return s;
    }

    DLLLOCAL const char* c_str() {
        return str ? str->c_str() : "";
    }

    DLLLOCAL QoreStringNode* getStr() {
        return str;
    }

private:
    QoreStringNode* str = nullptr;
};

struct qore_ftp_private {
    mutable QoreThreadLock m;
    QoreSocket control, data;
    char* host = nullptr,
        *user = nullptr,
        *pass = nullptr,
        *url_path = nullptr;

    int mode = FTP_MODE_UNKNOWN,
        port = DEFAULT_FTP_CONTROL_PORT,
        timeout_ms = 30000,  // 30-second timeout by default
        family = AF_UNSPEC;

    bool control_connected = false,
        loggedin = false,
        secure = false,
        secure_data = false,
        manual_mode = false;

    Queue* event_queue = nullptr;
    QoreValue event_arg;
    bool event_with_data = false;

    Queue* data_event_queue = nullptr;
    QoreValue data_event_arg;
    bool data_event_with_data = false;

    Queue* warning_queue = nullptr;
    QoreValue warning_arg;
    int64 warning_ms = 0,
        warning_bs = 0,
        warning_min_ms = 1000;

    // Async I/O — control channel poll operation (lazy-init)
    QoreObject* ctrl_op_obj = nullptr;
    FtpControlPollOperationPriv* ctrl_op = nullptr;
    std::string async_owner;  //!< per-instance owner for async controller cache

    DLLLOCAL qore_ftp_private(const QoreString* url, ExceptionSink* xsink) {
        char buf[32];
        snprintf(buf, sizeof(buf), "ftp-%p", (void*)this);
        async_owner = buf;
        if (url) {
            setURLIntern(url, xsink);
        }
    }

    DLLLOCAL qore_ftp_private() {
        char buf[32];
        snprintf(buf, sizeof(buf), "ftp-%p", (void*)this);
        async_owner = buf;
    }

    DLLLOCAL ~qore_ftp_private() {
        assert(!ctrl_op_obj);
        if (host) {
            free(host);
        }
        if (user) {
            free(user);
        }
        if (pass) {
            free(pass);
        }
        if (url_path) {
            free(url_path);
        }
        ExceptionSink xsink;
        clearStoredEventQueue(&xsink);
        clearStoredDataEventQueue(&xsink);
        clearStoredWarningQueue(&xsink);
    }

    DLLLOCAL void clearStoredEventQueue(ExceptionSink* xsink) {
        if (event_queue) {
            event_queue->deref(xsink);
            event_queue = nullptr;
        }
        if (event_arg) {
            event_arg.discard(xsink);
            event_arg.clear();
        }
        event_with_data = false;
    }

    DLLLOCAL void clearStoredDataEventQueue(ExceptionSink* xsink) {
        if (data_event_queue) {
            data_event_queue->deref(xsink);
            data_event_queue = nullptr;
        }
        if (data_event_arg) {
            data_event_arg.discard(xsink);
            data_event_arg.clear();
        }
        data_event_with_data = false;
    }

    DLLLOCAL void clearStoredWarningQueue(ExceptionSink* xsink) {
        if (warning_queue) {
            warning_queue->deref(xsink);
            warning_queue = nullptr;
        }
        if (warning_arg) {
            warning_arg.discard(xsink);
            warning_arg.clear();
        }
        warning_ms = 0;
        warning_bs = 0;
        warning_min_ms = 1000;
    }

    DLLLOCAL int applyEventQueue(QoreSocketObject* sock, Queue* q, QoreValue& arg, bool with_data,
            ExceptionSink* xsink) {
        if (!sock) {
            return 0;
        }
        if (q) {
            q->ref();
        }
        sock->setEventQueue(xsink, q, arg.refSelf(), with_data);
        return *xsink ? -1 : 0;
    }

    DLLLOCAL int applyWarningQueue(QoreSocketObject* sock, ExceptionSink* xsink) {
        if (!sock) {
            return 0;
        }
        if (!warning_queue) {
            sock->clearWarningQueue(xsink);
            return *xsink ? -1 : 0;
        }
        warning_queue->ref();
        sock->setWarningQueue(xsink, warning_ms, warning_bs, warning_queue, warning_arg.refSelf(), warning_min_ms);
        return *xsink ? -1 : 0;
    }

    DLLLOCAL int applyControlSocketQueues(QoreSocketObject* sock, ExceptionSink* xsink) {
        return applyEventQueue(sock, event_queue, event_arg, event_with_data, xsink)
            || applyWarningQueue(sock, xsink) ? -1 : 0;
    }

    DLLLOCAL int applyDataSocketQueues(QoreSocketObject* sock, ExceptionSink* xsink) {
        return applyEventQueue(sock, data_event_queue, data_event_arg, data_event_with_data, xsink)
            || applyWarningQueue(sock, xsink) ? -1 : 0;
    }

    //! Clean up async control op (must be called with ExceptionSink before destructor)
    DLLLOCAL void cleanupAsync(ExceptionSink* xsink) {
        if (ctrl_op_obj) {
            ExceptionSink cleanup_xsink;
            // Cancel our operations from the async controller cache before deref
            ReferenceHolder<QoreObject> ctl_obj(qore_get_async_io_controller_obj(&cleanup_xsink), &cleanup_xsink);
            if (!cleanup_xsink && *ctl_obj) {
                ReferenceHolder<AsyncIoControllerPriv> ctl_priv(
                    static_cast<AsyncIoControllerPriv*>(
                        (*ctl_obj)->getReferencedPrivateData(CID_ASYNCIOCONTROLLER, &cleanup_xsink)),
                    &cleanup_xsink);
                if (!cleanup_xsink && *ctl_priv) {
                    SimpleRefHolder<QoreStringNode> owner(new QoreStringNode(async_owner));
                    ctl_priv->cancelByOwner(*owner, &cleanup_xsink);
                }
            }
            if (cleanup_xsink) {
                cleanup_xsink.clear();
            }
            ctrl_op = nullptr;
            ctrl_op_obj->deref(&cleanup_xsink);
            if (cleanup_xsink) {
                cleanup_xsink.clear();
            }
            ctrl_op_obj = nullptr;
        }
    }

    DLLLOCAL void setNetworkFamily(int family) {
        this->family = family;
    }

    DLLLOCAL int getNetworkFamily() const {
        return family;
    }

    // private unlocked
    DLLLOCAL void setURLIntern(const QoreString* url_str, ExceptionSink* xsink) {
        QoreURL url(url_str);
        if (!url.getHost()) {
            xsink->raiseException("FTP-URL-ERROR", "no hostname given in URL '%s'", url_str->c_str());
            return;
        }

        // verify protocol
        if (url.getProtocol()) {
            if (!url.getProtocol()->compare("ftps"))
                secure = secure_data = true;
            else if (url.getProtocol()->compare("ftp")) {
                xsink->raiseException("UNSUPPORTED-PROTOCOL", "'%s' not supported (expected 'ftp' or 'ftps')",
                    url.getProtocol()->c_str());
                return;
            }
        }

        // set username
        user = url.take_username();
        // set password
        pass = url.take_password();
        // set host
        host = url.take_host();
        // set URL path
        url_path = url.take_path();
        // set port
        port = url.getPort() ? url.getPort() : DEFAULT_FTP_CONTROL_PORT;
    }

    DLLLOCAL QoreHashNode* getControlPeerInfo(ExceptionSink* xsink, bool host_lookup) const {
        AutoLocker al(m);
        if (ctrl_op && ctrl_op->getControlSocket()) {
            return ctrl_op->getControlSocket()->getPeerInfo(xsink, host_lookup);
        }
        return control.getPeerInfo(xsink, host_lookup);
    }

    DLLLOCAL QoreHashNode* getDataPeerInfo(ExceptionSink* xsink, bool host_lookup) const {
        AutoLocker al(m);
        return data.getPeerInfo(xsink, host_lookup);
    }

    DLLLOCAL QoreHashNode* getControlSocketInfo(ExceptionSink* xsink, bool host_lookup) const {
        AutoLocker al(m);
        if (ctrl_op && ctrl_op->getControlSocket()) {
            return ctrl_op->getControlSocket()->getSocketInfo(xsink, host_lookup);
        }
        return control.getSocketInfo(xsink, host_lookup);
    }

    DLLLOCAL QoreHashNode* getDataSocketInfo(ExceptionSink* xsink, bool host_lookup) const {
        AutoLocker al(m);
        return data.getSocketInfo(xsink, host_lookup);
    }

    DLLLOCAL const char* getSSLCipherName() const {
        AutoLocker al(m);
        QoreSocketObject* sock = getAsyncControlSocketUnlocked();
        return sock ? sock->getSSLCipherName() : control.getSSLCipherName();
    }

    DLLLOCAL const char* getSSLCipherVersion() const {
        AutoLocker al(m);
        QoreSocketObject* sock = getAsyncControlSocketUnlocked();
        return sock ? sock->getSSLCipherVersion() : control.getSSLCipherVersion();
    }

    DLLLOCAL long verifyPeerCertificate() const {
        AutoLocker al(m);
        QoreSocketObject* sock = getAsyncControlSocketUnlocked();
        return sock ? sock->verifyPeerCertificate() : control.verifyPeerCertificate();
    }

    DLLLOCAL bool isControlConnectedUnlocked() const {
        return ctrl_op && ctrl_op->isReady();
    }

    DLLLOCAL QoreSocketObject* getAsyncControlSocketUnlocked() const {
        return ctrl_op ? ctrl_op->getControlSocket() : nullptr;
    }

    DLLLOCAL void setControlEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
        AutoLocker al(m);
        clearStoredEventQueue(xsink);
        event_queue = q;
        event_arg = arg;
        event_with_data = with_data;
        if (event_queue) {
            event_queue->ref();
        }
        control.setEventQueue(xsink, event_queue, event_arg.refSelf(), event_with_data);
        if (*xsink) {
            return;
        }
        applyEventQueue(getAsyncControlSocketUnlocked(), event_queue, event_arg, event_with_data, xsink);
    }

    DLLLOCAL void setDataEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
        AutoLocker al(m);
        clearStoredDataEventQueue(xsink);
        data_event_queue = q;
        data_event_arg = arg;
        data_event_with_data = with_data;
        if (data_event_queue) {
            data_event_queue->ref();
        }
        data.setEventQueue(xsink, data_event_queue, data_event_arg.refSelf(), data_event_with_data);
    }

    DLLLOCAL void setEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
        AutoLocker al(m);
        clearStoredEventQueue(xsink);
        clearStoredDataEventQueue(xsink);
        event_queue = q;
        event_arg = arg;
        event_with_data = with_data;
        if (event_queue) {
            event_queue->ref();
            data_event_queue = event_queue;
            data_event_arg = event_arg.refSelf();
            data_event_with_data = with_data;
        }

        if (event_queue) {
            event_queue->ref();
        }
        control.setEventQueue(xsink, event_queue, event_arg.refSelf(), event_with_data);
        if (*xsink) {
            return;
        }
        if (event_queue) {
            event_queue->ref();
        }
        data.setEventQueue(xsink, event_queue, event_arg.refSelf(), event_with_data);
        if (*xsink) {
            return;
        }
        applyEventQueue(getAsyncControlSocketUnlocked(), event_queue, event_arg, event_with_data, xsink);
    }

    DLLLOCAL void cleanup(ExceptionSink* xsink) {
        AutoLocker al(m);
        if (data.getQueue() && (data.getQueue() == control.getQueue())) {
            // make sure only one close event is pushed on the queue
            data.cleanup(xsink);
            control.setEventQueue(xsink, nullptr, QoreValue(), false);
            return;
        }

        data.cleanup(xsink);
        control.cleanup(xsink);
    }

    // unlocked
    DLLLOCAL int checkConnectedUnlocked(ExceptionSink* xsink) {
        // Single chokepoint — every public sync FtpClient entry point routes
        // through this helper before touching the control/data sockets, so
        // one assert here catches any I/O-thread misuse at the FtpClient API
        // level with a clear class name in the error message.
        SocketSyncPoll::assertNotOnIoThread("FtpClient", "ftp", xsink);
        return (!loggedin || !isControlConnectedUnlocked()) && connectUnlocked(xsink) ? -1 : 0;
    }

    DLLLOCAL void disconnectIntern() {
        control.close();
        control_connected = false;
        if (!manual_mode) {
            mode = FTP_MODE_UNKNOWN;
        }
        data.close();
        loggedin = false;
        // Clear async pointers. Active controller submissions are canceled by
        // cleanupAsync() on error paths before this object is released.
        ctrl_op = nullptr;
        if (ctrl_op_obj) {
            ExceptionSink xsink;
            ctrl_op_obj->deref(&xsink);
            ctrl_op_obj = nullptr;
        }
    }

    DLLLOCAL int connect(ExceptionSink* xsink) {
        SocketSyncPoll::assertNotOnIoThread("FtpClient", "connect", xsink);
        SafeLocker sl(m);
        return connectUnlocked(xsink);
    }

    // unlocked
    DLLLOCAL QoreStringNode* sendMsg(int &code, const char* cmd, const char* arg, ExceptionSink* xsink) {
        return sendMsgAsyncBlocking(code, cmd, arg, xsink);
    }

    //! Async connect: creates FtpControlPollOperation and executes it on the controller
    DLLLOCAL int connectAsyncBlocking(ExceptionSink* xsink) {
        disconnectIntern();
        if (!host) {
            xsink->raiseException("FTP-CONNECT-ERROR", "no hostname set");
            return -1;
        }

        // Create FtpControlPollOperation — this creates the socket and starts the connect
        // We need to create the QoreObject + priv manually from C++.
        //
        // Refcount plan: new=1 (consumed below by `new QoreObject(QC_SOCKET...)`
        // which takes ownership of the priv); +1 ref here for the connect op.
        QoreSocketObject* sock_priv = new QoreSocketObject;
        if (applyControlSocketQueues(sock_priv, xsink)) {
            sock_priv->deref(xsink);
            return -1;
        }

        QoreStringMaker target("%s:%d", host, port);
        sock_priv->ref();  // ref for connect op
        SocketConnectPollOperation* connect_op = new SocketConnectPollOperation(xsink,
            false, target.c_str(), sock_priv, true);
        if (*xsink) {
            connect_op->deref(xsink);
            sock_priv->deref(xsink);
            return -1;
        }

        // Create QoreObject wrapper for the poll operation
        ctrl_op_obj = new QoreObject(QC_FTPCONTROLPOLLOPERATION, getProgram());
        // Set sock member (setMemberValue takes ownership)
        QoreObject* sock_obj = new QoreObject(QC_SOCKET, getProgram(), sock_priv);
        ctrl_op_obj->setMemberValue("sock", QC_FTPCONTROLPOLLOPERATION, sock_obj, xsink);
        if (*xsink) {
            sock_obj->deref(xsink);
            connect_op->deref(xsink);
            ctrl_op_obj->deref(xsink);
            ctrl_op_obj = nullptr;
            return -1;
        }
        ctrl_op_obj->setMemberValue("goal", QC_FTPCONTROLPOLLOPERATION,
            new QoreStringNode("ftp-login"), xsink);
        if (*xsink) {
            connect_op->deref(xsink);
            ctrl_op_obj->deref(xsink);
            ctrl_op_obj = nullptr;
            return -1;
        }

        // Create C++ priv and set it on the QoreObject
        ctrl_op = new FtpControlPollOperationPriv(ctrl_op_obj, sock_priv, connect_op,
            secure, secure_data,
            user ? user : nullptr,
            pass ? pass : nullptr);
        ctrl_op_obj->setPrivate(CID_FTPCONTROLPOLLOPERATION, ctrl_op);
        // Set self on the connect op so it can create poll info
        connect_op->setSelf(ctrl_op_obj);

        ReferenceHolder<QoreHashNode> result(
            ctrl_op->execOnController(xsink, async_owner.c_str(), timeout_ms, true), xsink);
        if (*xsink || qore_ftp_check_controller_result(*result, "FTP login", xsink)) {
            cleanupAsync(xsink);
            return -1;
        }

        if (ctrl_op->isClosed()) {
            xsink->raiseException("FTP-CONNECT-ERROR", "FTP connection closed during login");
            cleanupAsync(xsink);
            return -1;
        }
        if (!ctrl_op->isReady()) {
            xsink->raiseException("FTP-CONNECT-ERROR", "FTP login did not complete");
            cleanupAsync(xsink);
            return -1;
        }

        loggedin = true;
        control_connected = true;
        return 0;
    }

    //! Async sendMsg: submits command via async poll op, blocks for response
    DLLLOCAL QoreStringNode* sendMsgAsyncBlocking(int& code, const char* cmd, const char* arg,
            ExceptionSink* xsink) {
        if (!ctrl_op || !ctrl_op->isReady()) {
            xsink->raiseException("FTP-SEND-ERROR", "not connected (async)");
            return nullptr;
        }

        ctrl_op->submitCommand(cmd, arg, xsink);
        if (*xsink) {
            return nullptr;
        }

        ReferenceHolder<QoreHashNode> result(
            ctrl_op->execOnController(xsink, async_owner.c_str(), timeout_ms, true), xsink);
        if (*xsink || qore_ftp_check_controller_result(*result, "FTP command", xsink)) {
            return nullptr;
        }

        code = ctrl_op->getLastResponseCode();
        // Get the response text from getOutput()
        ValueHolder output(ctrl_op->getOutput(), xsink);
        if (output->getType() == NT_HASH) {
            QoreHashNode* h = output->get<QoreHashNode>();
            QoreValue msg = h->getKeyValue("message");
            if (msg.getType() == NT_STRING) {
                return msg.get<QoreStringNode>()->stringRefSelf();
            }
        }
        return new QoreStringNode("");
    }

    //! Read a response from the control channel without sending a command (async)
    DLLLOCAL QoreStringNode* recvResponseAsyncBlocking(int& code, ExceptionSink* xsink) {
        if (!ctrl_op || !ctrl_op->isReady()) {
            xsink->raiseException("FTP-RECEIVE-ERROR", "not connected (async)");
            return nullptr;
        }
        ctrl_op->submitRecvResponse(xsink);
        if (*xsink) {
            return nullptr;
        }
        ReferenceHolder<QoreHashNode> result(
            ctrl_op->execOnController(xsink, async_owner.c_str(), timeout_ms, true), xsink);
        if (*xsink || qore_ftp_check_controller_result(*result, "FTP response receive", xsink)) {
            return nullptr;
        }
        code = ctrl_op->getLastResponseCode();
        ValueHolder output(ctrl_op->getOutput(), xsink);
        if (output->getType() == NT_HASH) {
            QoreHashNode* h = output->get<QoreHashNode>();
            QoreValue msg = h->getKeyValue("message");
            if (msg.getType() == NT_STRING) {
                return msg.get<QoreStringNode>()->stringRefSelf();
            }
        }
        return new QoreStringNode("");
    }

    //! Create a FtpDataPollOperation and submit it to the async controller
    /** Returns the data op priv, the operation object, and the controller result
        queue. Does NOT block — the caller must send the transfer command
        (RETR/STOR/LIST) on the control channel and then wait on the queue.
    */
    DLLLOCAL FtpDataPollOperationPriv* createAndSubmitDataOp(
            const char* data_host, int data_port, bool recv_mode,
            BinaryNode* send_data, QoreObject*& data_op_obj_out,
            QoreObject*& data_queue_obj_out,
            ExceptionSink* xsink) {
        // Create data socket.
        // Refcount plan: new=1 (consumed below by `new QoreObject(QC_SOCKET...)`
        // which takes ownership of the priv); +1 ref here for the connect op.
        QoreSocketObject* dsock = new QoreSocketObject;
        if (applyDataSocketQueues(dsock, xsink)) {
            dsock->deref(xsink);
            return nullptr;
        }

        QoreStringMaker dtarget("%s:%d", data_host, data_port);
        dsock->ref();  // for connect op
        SocketConnectPollOperation* dconnect = new SocketConnectPollOperation(xsink,
            false, dtarget.c_str(), dsock, true);
        if (*xsink) {
            dconnect->deref(xsink);
            dsock->deref(xsink);
            return nullptr;
        }

        // Create QoreObject for the data poll op
        QoreObject* dop_obj = new QoreObject(QC_FTPDATAPOLLOPERATION, getProgram());
        QoreObject* dsock_obj = new QoreObject(QC_SOCKET, getProgram(), dsock);
        dop_obj->setMemberValue("sock", QC_FTPDATAPOLLOPERATION, dsock_obj, xsink);
        if (*xsink) {
            dsock_obj->deref(xsink);
            dconnect->deref(xsink);
            dop_obj->deref(xsink);
            return nullptr;
        }
        dop_obj->setMemberValue("goal", QC_FTPDATAPOLLOPERATION,
            new QoreStringNode(recv_mode ? "ftp-data-recv" : "ftp-data-send"), xsink);
        if (*xsink) {
            dconnect->deref(xsink);
            dop_obj->deref(xsink);
            return nullptr;
        }

        FtpDataPollOperationPriv* dop;
        if (recv_mode) {
            dop = new FtpDataPollOperationPriv(dop_obj, dsock, dconnect, secure_data);
        } else {
            dop = new FtpDataPollOperationPriv(dop_obj, dsock, dconnect, secure_data, send_data);
        }
        dop_obj->setPrivate(CID_FTPDATAPOLLOPERATION, dop);
        // Set self on the connect op so it can create poll info
        dconnect->setSelf(dop_obj);

        // Submit to controller (non-blocking — starts connect immediately).
        // The returned queue is the sync caller's completion handle.
        QoreObject* queue_obj = dop->submitToController(xsink, async_owner.c_str(), timeout_ms, false);
        if (*xsink || !queue_obj) {
            if (!*xsink) {
                xsink->raiseException("FTP-DATA-ERROR", "failed to submit data operation to AsyncIoController");
            }
            dop_obj->deref(xsink);
            return nullptr;
        }

        data_op_obj_out = dop_obj;
        data_queue_obj_out = queue_obj;
        return dop;
    }

    //! Parse EPSV response "229 ... (|||port|)" → returns data port, or -1 on error
    DLLLOCAL int parseEpsvResponse(const char* resp, ExceptionSink* xsink) {
        const char* s = strstr(resp, "|||");
        if (!s) {
            xsink->raiseException("FTP-RESPONSE-ERROR", "cannot find port in EPSV response: %s", resp);
            return -1;
        }
        s += 3;
        const char* end = strchr(s, '|');
        if (!end) {
            xsink->raiseException("FTP-RESPONSE-ERROR", "cannot find port in EPSV response: %s", resp);
            return -1;
        }
        return atoi(s);
    }

    //! Parse PASV response "227 ... (h1,h2,h3,h4,p1,p2)" → fills host/port
    DLLLOCAL int parsePasvResponse(const char* resp, std::string& data_host_out,
            int& data_port_out, ExceptionSink* xsink) {
        const char* s = strstr(resp, "(");
        if (!s) {
            xsink->raiseException("FTP-RESPONSE-ERROR", "cannot parse PASV response: %s", resp);
            return -1;
        }
        int num[5];
        s++;
        for (int i = 0; i < 5; i++) {
            const char* comma = strchr(s, ',');
            if (!comma) {
                xsink->raiseException("FTP-RESPONSE-ERROR", "cannot parse PASV response: %s", resp);
                return -1;
            }
            num[i] = atoi(s);
            s = comma + 1;
        }
        data_port_out = (num[4] << 8) + atoi(s);
        char buf[32];
        snprintf(buf, sizeof(buf), "%d.%d.%d.%d", num[0], num[1], num[2], num[3]);
        data_host_out = buf;
        return 0;
    }

    //! Negotiate data channel: sends EPSV/PASV via async, returns host+port
    DLLLOCAL int negotiateDataChannelAsync(std::string& data_host_out,
            int& data_port_out, ExceptionSink* xsink) {
        int code;
        // Try EPSV first (unless mode is forced)
        if (mode == FTP_MODE_UNKNOWN || mode == FTP_MODE_EPSV) {
            QoreStringNodeHolder resp(sendMsgAsyncBlocking(code, "EPSV", nullptr, xsink));
            if (*xsink) {
                return -1;
            }
            if ((code / 100) == 2) {
                int dp = parseEpsvResponse(resp->c_str(), xsink);
                if (*xsink) {
                    return -1;
                }
                data_host_out = host;
                data_port_out = dp;
                mode = FTP_MODE_EPSV;
                return 0;
            }
            // EPSV failed — if it was forced, error out
            if (mode == FTP_MODE_EPSV) {
                xsink->raiseException("FTP-CONNECT-ERROR",
                    "EPSV failed: %s", resp ? resp->c_str() : "no response");
                return -1;
            }
        }
        // Try PASV
        if (mode == FTP_MODE_UNKNOWN || mode == FTP_MODE_PASV) {
            QoreStringNodeHolder resp(sendMsgAsyncBlocking(code, "PASV", nullptr, xsink));
            if (*xsink) {
                return -1;
            }
            if ((code / 100) != 2) {
                xsink->raiseException("FTP-CONNECT-ERROR",
                    "PASV failed: %s", resp ? resp->c_str() : "no response");
                return -1;
            }
            if (parsePasvResponse(resp->c_str(), data_host_out, data_port_out, xsink)) {
                return -1;
            }
            mode = FTP_MODE_PASV;
            return 0;
        }
        // PORT mode: handled separately via portTransferAsyncBlocking
        xsink->raiseException("FTP-CONNECT-ERROR",
            "PORT mode must use portTransferAsyncBlocking directly");
        return -1;
    }

    //! PORT mode async data transfer: bind+listen+PORT+accept+transfer, all non-blocking
    /** @param transfer_cmd "RETR", "STOR", or "LIST"/"NLST"
        @param transfer_arg command argument (file path)
        @param send_data data to send (for STOR), nullptr for recv
        @param recv_output receives downloaded data (for RETR/LIST), nullptr for send
        @return 0 on success, -1 on error
    */
    DLLLOCAL int portTransferAsyncBlocking(const char* transfer_cmd, const char* transfer_arg,
            const void* send_data_ptr, size_t send_len,
            BinaryNode** recv_output, ExceptionSink* xsink) {
        // 1. Get local interface address from the async control socket
        int ctrl_fd = (ctrl_op && ctrl_op->getControlSocket())
            ? ctrl_op->getControlSocket()->getSocket()
            : control.getSocket();
        struct sockaddr_in add;
        socklen_t socksize = sizeof(struct sockaddr_in);
        if (getsockname(ctrl_fd, (struct sockaddr*)&add, &socksize) < 0) {
            xsink->raiseErrnoException("FTP-CONNECT-ERROR", errno,
                "cannot determine local interface address");
            return -1;
        }

        // 2. Create a fresh QoreSocketObject for listening, bind + listen.
        // Refcount plan: new=1 (consumed below by `new QoreObject(QC_SOCKET...)`
        // which takes ownership of the priv); +1 ref for the accept op added
        // further down right before SocketAcceptPollOperation creation.
        QoreSocketObject* listen_sock = new QoreSocketObject;
        if (applyDataSocketQueues(listen_sock, xsink)) {
            listen_sock->deref(xsink);
            return -1;
        }
        char ifname_buf[80];
        if (!inet_ntop(AF_INET, &add.sin_addr, ifname_buf, sizeof(ifname_buf))) {
            listen_sock->deref(xsink);
            xsink->raiseErrnoException("FTP-CONNECT-ERROR", errno,
                "cannot determine local interface address");
            return -1;
        }
        // Bind on the control connection's local IPv4 interface, ephemeral port
        if (listen_sock->bindINET(ifname_buf, "0", true, Q_AF_INET, Q_SOCK_STREAM, 0, xsink)) {
            listen_sock->deref(xsink);
            return -1;
        }
        int dataport = listen_sock->getPort();
        char ifname[80];
        strncpy(ifname, ifname_buf, sizeof(ifname) - 1);
        ifname[sizeof(ifname) - 1] = '\0';
        if (listen_sock->listen(5)) {
            listen_sock->deref(xsink);
            return -1;
        }

        // Wrap in QoreObject for async controller
        QoreObject* listen_obj = new QoreObject(QC_SOCKET, getProgram(), listen_sock);
        ReferenceHolder<QoreObject> listen_holder(listen_obj, xsink);

        // 3. Create FtpPortAcceptPollOperation and submit to controller FIRST
        //    (must be polling before RETR triggers the server's connect-back)
        listen_sock->ref();  // ref for SocketAcceptPollOperation
        SocketAcceptPollOperation* raw_accept = new SocketAcceptPollOperation(xsink, listen_sock);
        if (*xsink) {
            raw_accept->deref(xsink);
            return -1;
        }

        // Create FtpPortAcceptPollOperation (composite: accept + optional SSL)
        QoreObject* accept_op_obj = new QoreObject(QC_FTPPORTACCEPTPOLLOPERATION, getProgram());
        listen_obj->ref();
        accept_op_obj->setMemberValue("sock", QC_FTPPORTACCEPTPOLLOPERATION, listen_obj, xsink);
        accept_op_obj->setMemberValue("goal", QC_FTPPORTACCEPTPOLLOPERATION,
            new QoreStringNode("ftp-port-accept"), xsink);
        if (*xsink) {
            raw_accept->deref(xsink);
            accept_op_obj->deref(xsink);
            return -1;
        }
        FtpPortAcceptPollOperationPriv* accept_priv = new FtpPortAcceptPollOperationPriv(
            accept_op_obj, raw_accept, listen_sock, secure_data);
        accept_op_obj->setPrivate(CID_FTPPORTACCEPTPOLLOPERATION, accept_priv);
        raw_accept->setSelf(accept_op_obj);
        ReferenceHolder<QoreObject> accept_holder(accept_op_obj, xsink);

        // Submit accept op to controller (non-blocking — starts polling listener fd)
        ReferenceHolder<QoreObject> accept_queue(
            accept_priv->submitToController(xsink, async_owner.c_str(), timeout_ms, false), xsink);
        if (*xsink || !accept_queue) {
            if (!*xsink) {
                xsink->raiseException("FTP-CONNECT-ERROR", "failed to submit PORT accept operation");
            }
            return -1;
        }

        // 4. Now send PORT command (server learns where to connect)
        char ifcopy[80];
        strncpy(ifcopy, ifname, sizeof(ifcopy) - 1);
        ifcopy[sizeof(ifcopy) - 1] = '\0';
        for (int i = 0; ifcopy[i]; i++) {
            if (ifcopy[i] == '.') {
                ifcopy[i] = ',';
            }
        }
        QoreString pconn;
        pconn.sprintf("%s,%d,%d", ifcopy, dataport >> 8, dataport & 255);
        int code;
        QoreStringNodeHolder mr(sendMsgAsyncBlocking(code, "PORT", pconn.c_str(), xsink));
        if (*xsink || (code / 100) != 2) {
            if (!*xsink) {
                xsink->raiseException("FTP-CONNECT-ERROR",
                    "PORT command failed: %s", mr ? mr->c_str() : "no response");
            }
            return -1;
        }

        // 5. Send transfer command — triggers server connect-back to our PORT
        mr = sendMsgAsyncBlocking(code, transfer_cmd, transfer_arg, xsink);
        if (*xsink || (code / 100) != 1) {
            if (!*xsink) {
                xsink->raiseException("FTP-DATA-ERROR",
                    "%s failed: %s", transfer_cmd, mr ? mr->c_str() : "no response");
            }
            return -1;
        }

        // 6. Block until accept (+ optional SSL) completes
        if (qore_ftp_wait_controller_queue(*accept_queue, "FTP PORT accept", xsink)) {
            return -1;
        }

        // 7. Get accepted socket
        ValueHolder accepted_val(accept_priv->getOutput(), xsink);
        if (accepted_val->getType() != NT_OBJECT) {
            xsink->raiseException("FTP-CONNECT-ERROR", "PORT accept did not return a socket");
            return -1;
        }
        QoreObject* accepted_obj = accepted_val->get<QoreObject>();
        // getReferencedPrivateData returns the priv with a +1 ref.  Wrap in
        // a SimpleRefHolder so we own the release on every exit path: the
        // FtpDataPollOperationPriv ctor stores `data_sock` as a raw pointer
        // without ref/deref, and the underlying priv is also kept alive by
        // the "sock" member QoreObject for as long as data_op_obj exists, so
        // it is safe to release our local ref here once the priv is adopted.
        SimpleRefHolder<QoreSocketObject> accepted_sock_holder(
            static_cast<QoreSocketObject*>(
                accepted_obj->getReferencedPrivateData(CID_SOCKET, xsink)));
        QoreSocketObject* accepted_sock = *accepted_sock_holder;
        if (*xsink || !accepted_sock) {
            if (!*xsink) {
                xsink->raiseException("FTP-CONNECT-ERROR", "failed to get accepted socket");
            }
            return -1;
        }
        if (applyDataSocketQueues(accepted_sock, xsink)) {
            return -1;
        }

        // 8. Create FtpDataPollOperation with adopt-socket constructor.
        QoreObject* data_op_obj = new QoreObject(QC_FTPDATAPOLLOPERATION, getProgram());
        accepted_obj->ref();
        data_op_obj->setMemberValue("sock", QC_FTPDATAPOLLOPERATION, accepted_obj, xsink);
        data_op_obj->setMemberValue("goal", QC_FTPDATAPOLLOPERATION,
            new QoreStringNode(recv_output ? "ftp-data-recv" : "ftp-data-send"), xsink);
        if (*xsink) {
            data_op_obj->deref(xsink);
            return -1;
        }

        FtpDataPollOperationPriv* data_op;
        if (send_data_ptr && send_len > 0) {
            BinaryNode* send_bin = new BinaryNode;
            send_bin->append(send_data_ptr, send_len);
            data_op = new FtpDataPollOperationPriv(data_op_obj, accepted_sock,
                secure_data, send_bin);
        } else {
            data_op = new FtpDataPollOperationPriv(data_op_obj, accepted_sock,
                secure_data, true);
        }
        data_op_obj->setPrivate(CID_FTPDATAPOLLOPERATION, data_op);
        ReferenceHolder<QoreObject> data_op_holder(data_op_obj, xsink);

        // Submit data transfer to controller and block
        ReferenceHolder<QoreObject> data_queue(
            data_op->submitToController(xsink, async_owner.c_str(), timeout_ms, false), xsink);
        if (*xsink || !data_queue) {
            if (!*xsink) {
                xsink->raiseException("FTP-DATA-ERROR", "failed to submit PORT data operation");
            }
            return -1;
        }
        if (qore_ftp_wait_controller_queue(*data_queue, "FTP PORT data transfer", xsink)) {
            return -1;
        }

        // 9. Get received data
        if (recv_output) {
            ValueHolder result(data_op->getOutput(), xsink);
            if (result->getType() == NT_BINARY) {
                *recv_output = result.release().get<BinaryNode>();
            } else {
                *recv_output = new BinaryNode;
            }
        }

        // 10. Read 226 completion response
        mr = recvResponseAsyncBlocking(code, xsink);
        if (*xsink) {
            return -1;
        }
        if ((code / 100) != 2) {
            xsink->raiseException("FTP-DATA-ERROR",
                "transfer completion failed: %s", mr ? mr->c_str() : "no response");
            return -1;
        }
        return 0;
    }

    //! Async GET: download a file via async I/O controller
    DLLLOCAL BinaryNode* getAsyncBlocking(const char* remotepath, ExceptionSink* xsink) {
        // 1. TYPE I (binary mode)
        int code;
        QoreStringNodeHolder resp(sendMsgAsyncBlocking(code, "TYPE", "I", xsink));
        if (*xsink || (code / 100) != 2) {
            if (!*xsink) {
                xsink->raiseException("FTP-ERROR", "TYPE I failed: %s", resp ? resp->c_str() : "no response");
            }
            return nullptr;
        }

        // PORT mode: use dedicated PORT transfer flow
        if (mode == FTP_MODE_PORT) {
            BinaryNode* output = nullptr;
            if (portTransferAsyncBlocking("RETR", remotepath, nullptr, 0, &output, xsink)) {
                return nullptr;
            }
            return output ? output : new BinaryNode;
        }

        // 2. Negotiate data channel (EPSV/PASV)
        std::string dhost;
        int dport;
        if (negotiateDataChannelAsync(dhost, dport, xsink)) {
            return nullptr;
        }

        // 3. Create and submit data receive op (non-blocking — starts connecting)
        QoreObject* data_op_obj = nullptr;
        QoreObject* data_queue_obj = nullptr;
        FtpDataPollOperationPriv* data_op = createAndSubmitDataOp(
            dhost.c_str(), dport, true, nullptr, data_op_obj, data_queue_obj, xsink);
        if (!data_op) {
            return nullptr;
        }
        ReferenceHolder<QoreObject> data_op_holder(data_op_obj, xsink);
        ReferenceHolder<QoreObject> data_queue_holder(data_queue_obj, xsink);

        // 4. RETR command on control channel
        resp = sendMsgAsyncBlocking(code, "RETR", remotepath, xsink);
        if (*xsink || (code / 100) != 1) {
            if (!*xsink) {
                xsink->raiseException("FTP-GET-ERROR",
                    "RETR failed: %s", resp ? resp->c_str() : "no response");
            }
            return nullptr;
        }

        // 5. Wait for data transfer to complete
        if (qore_ftp_wait_controller_queue(*data_queue_holder, "FTP data transfer", xsink)) {
            return nullptr;
        }

        // 6. Get received data
        ValueHolder result(data_op->getOutput(), xsink);

        // 7. Read 226 completion response
        resp = recvResponseAsyncBlocking(code, xsink);
        if (*xsink) {
            return nullptr;
        }
        if ((code / 100) != 2) {
            xsink->raiseException("FTP-GET-ERROR",
                "transfer completion failed: %s", resp ? resp->c_str() : "no response");
            return nullptr;
        }

        if (result->getType() == NT_BINARY) {
            return result.release().get<BinaryNode>();
        }
        return new BinaryNode;
    }

    //! Async PUT: upload data via async I/O controller
    DLLLOCAL int putAsyncBlocking(const void* data_ptr, size_t data_len,
            const char* remotename, ExceptionSink* xsink) {
        // 1. TYPE I
        int code;
        QoreStringNodeHolder resp(sendMsgAsyncBlocking(code, "TYPE", "I", xsink));
        if (*xsink || (code / 100) != 2) {
            if (!*xsink) {
                xsink->raiseException("FTP-ERROR", "TYPE I failed: %s", resp ? resp->c_str() : "no response");
            }
            return -1;
        }

        // PORT mode
        if (mode == FTP_MODE_PORT) {
            return portTransferAsyncBlocking("STOR", remotename,
                data_ptr, data_len, nullptr, xsink);
        }

        // 2. Negotiate data channel
        std::string dhost;
        int dport;
        if (negotiateDataChannelAsync(dhost, dport, xsink)) {
            return -1;
        }

        // 3. Create send data binary
        BinaryNode* send_bin = new BinaryNode;
        send_bin->append(data_ptr, data_len);

        // 4. Create and submit data send op
        QoreObject* data_op_obj = nullptr;
        QoreObject* data_queue_obj = nullptr;
        FtpDataPollOperationPriv* data_op = createAndSubmitDataOp(
            dhost.c_str(), dport, false, send_bin, data_op_obj, data_queue_obj, xsink);
        if (!data_op) {
            send_bin->deref();
            return -1;
        }
        ReferenceHolder<QoreObject> data_op_holder(data_op_obj, xsink);
        ReferenceHolder<QoreObject> data_queue_holder(data_queue_obj, xsink);

        // 5. STOR command
        resp = sendMsgAsyncBlocking(code, "STOR", remotename, xsink);
        if (*xsink || (code / 100) != 1) {
            if (!*xsink) {
                xsink->raiseException("FTP-PUT-ERROR",
                    "STOR failed: %s", resp ? resp->c_str() : "no response");
            }
            return -1;
        }

        // 6. Wait for data transfer
        if (qore_ftp_wait_controller_queue(*data_queue_holder, "FTP data transfer", xsink)) {
            return -1;
        }

        // 7. Read 226
        resp = recvResponseAsyncBlocking(code, xsink);
        if (*xsink) {
            return -1;
        }
        if ((code / 100) != 2) {
            xsink->raiseException("FTP-PUT-ERROR",
                "transfer completion failed: %s", resp ? resp->c_str() : "no response");
            return -1;
        }
        return 0;
    }

    //! Async LIST: directory listing via async I/O controller
    DLLLOCAL QoreStringNode* listAsyncBlocking(const char* path, bool long_list,
            ExceptionSink* xsink) {
        // 1. TYPE A (ASCII)
        int code;
        QoreStringNodeHolder resp(sendMsgAsyncBlocking(code, "TYPE", "A", xsink));
        if (*xsink || (code / 100) != 2) {
            if (!*xsink) {
                xsink->raiseException("FTP-ERROR", "TYPE A failed: %s", resp ? resp->c_str() : "no response");
            }
            return nullptr;
        }

        // PORT mode
        if (mode == FTP_MODE_PORT) {
            BinaryNode* output = nullptr;
            const char* cmd = long_list ? "LIST" : "NLST";
            if (portTransferAsyncBlocking(cmd, (path && *path) ? path : nullptr,
                    nullptr, 0, &output, xsink)) {
                return nullptr;
            }
            if (output) {
                SimpleRefHolder<BinaryNode> holder(output);
                return new QoreStringNode(static_cast<const char*>(output->getPtr()),
                    output->size(), QCS_DEFAULT);
            }
            return new QoreStringNode("");
        }

        // 2. Negotiate data channel
        std::string dhost;
        int dport;
        if (negotiateDataChannelAsync(dhost, dport, xsink)) {
            return nullptr;
        }

        // 3. Create data receive op
        QoreObject* data_op_obj = nullptr;
        QoreObject* data_queue_obj = nullptr;
        FtpDataPollOperationPriv* data_op = createAndSubmitDataOp(
            dhost.c_str(), dport, true, nullptr, data_op_obj, data_queue_obj, xsink);
        if (!data_op) {
            return nullptr;
        }
        ReferenceHolder<QoreObject> data_op_holder(data_op_obj, xsink);
        ReferenceHolder<QoreObject> data_queue_holder(data_queue_obj, xsink);

        // 4. LIST/NLST command
        resp = sendMsgAsyncBlocking(code, long_list ? "LIST" : "NLST",
            (path && *path) ? path : nullptr, xsink);
        if (*xsink) {
            return nullptr;
        }
        // 5xx = file not found
        if ((code / 100) == 5) {
            return nullptr;
        }
        if ((code / 100) != 1) {
            xsink->raiseException("FTP-LIST-ERROR",
                "LIST/NLST failed: %s", resp ? resp->c_str() : "no response");
            return nullptr;
        }

        // 5. Wait for data transfer
        if (qore_ftp_wait_controller_queue(*data_queue_holder, "FTP data transfer", xsink)) {
            return nullptr;
        }

        // 6. Get data
        ValueHolder result(data_op->getOutput(), xsink);

        // 7. Read 226
        resp = recvResponseAsyncBlocking(code, xsink);
        if (*xsink) {
            return nullptr;
        }
        if ((code / 100) != 2) {
            xsink->raiseException("FTP-LIST-ERROR",
                "transfer completion failed: %s", resp ? resp->c_str() : "no response");
            return nullptr;
        }

        // Convert binary to string
        if (result->getType() == NT_BINARY) {
            const BinaryNode* bin = result->get<const BinaryNode>();
            return new QoreStringNode(static_cast<const char*>(bin->getPtr()), bin->size(),
                QCS_DEFAULT);
        }
        return new QoreStringNode("");
    }

    DLLLOCAL int connectUnlocked(ExceptionSink* xsink) {
        return connectAsyncBlocking(xsink);
    }

    DLLLOCAL void disconnect() {
        m.lock();
        disconnectIntern();
        m.unlock();
    }

    DLLLOCAL void clearWarningQueue(ExceptionSink* xsink) {
        AutoLocker al(m);
        clearStoredWarningQueue(xsink);
        control.clearWarningQueue(xsink);
        data.clearWarningQueue(xsink);
        applyWarningQueue(getAsyncControlSocketUnlocked(), xsink);
    }

    DLLLOCAL void setWarningQueue(ExceptionSink* xsink, int64 warning_ms, int64 warning_bs, Queue* wq, QoreValue arg, int64 min_ms) {
        AutoLocker al(m);
        ReferenceHolder<Queue> qholder(wq, xsink);
        ValueHolder holder(arg, xsink);
        if (warning_ms <= 0 && warning_bs <= 0) {
            xsink->raiseException("SOCKET-SETWARNINGQUEUE-ERROR", "FtpClient::setWarningQueue() at least one of "
                "warning ms argument: " QLLD " and warning B/s argument: " QLLD " must be greater than zero; to "
                "clear, call FtpClient::clearWarningQueue() with no arguments", warning_ms, warning_bs);
            return;
        }
        if (warning_ms < 0) {
            warning_ms = 0;
        }
        if (warning_bs < 0) {
            warning_bs = 0;
        }

        clearStoredWarningQueue(xsink);
        warning_queue = qholder.release();
        warning_arg = holder.release();
        this->warning_ms = warning_ms;
        this->warning_bs = warning_bs;
        warning_min_ms = min_ms;

        if (warning_queue) {
            warning_queue->ref();
        }
        control.setWarningQueue(xsink, warning_ms, warning_bs, warning_queue, warning_arg.refSelf(), min_ms);
        if (!*xsink) {
            if (warning_queue) {
                warning_queue->ref();
            }
            data.setWarningQueue(xsink, warning_ms, warning_bs, warning_queue, warning_arg.refSelf(), min_ms);
        }
        if (!*xsink) {
            applyWarningQueue(getAsyncControlSocketUnlocked(), xsink);
        }
    }

    DLLLOCAL QoreHashNode* getUsageInfo() const {
        AutoLocker al(m);
        QoreHashNode* h = new QoreHashNode(autoTypeInfo);
        qore_socket_private::getUsageInfo(control, *h, data);
        return h;
    }

    DLLLOCAL void clearStats() {
        AutoLocker al(m);
    }

    DLLLOCAL static qore_ftp_private* get(QoreFtpClient& ftp) {
        return ftp.priv;
    }

    DLLLOCAL static qore_ftp_private* get(const QoreFtpClient& ftp) {
        return ftp.priv;
    }
};

const char* QoreFtpClientClass::getUrlPath() const {
    return qore_ftp_private::get(*this)->url_path;
}

QoreFtpClient::QoreFtpClient(const QoreString* url, ExceptionSink* xsink) : priv(new qore_ftp_private(url, xsink)) {
}

QoreFtpClient::QoreFtpClient() : priv(new qore_ftp_private) {
}

QoreFtpClient::~QoreFtpClient() {
   priv->disconnectIntern();
   delete priv;
}

static inline int getFTPCode(QoreString* str) {
   if (str->strlen() < 3)
      return -1;
   const char* b = str->c_str();
   return (b[0] - 48) * 100 + (b[1] - 48) * 10 + (b[0] - 48);
}

// public locked
int QoreFtpClient::disconnect() {
   priv->disconnect();
   return 0;
}

// public locked
int QoreFtpClient::connect(ExceptionSink* xsink) {
   return priv->connect(xsink);
}

// public locked
QoreHashNode* QoreFtpClient::sendControlMessage(const char* cmd, const char* arg, ExceptionSink* xsink) {
    SafeLocker sl(priv->m);
    if (priv->checkConnectedUnlocked(xsink)) {
        return nullptr;
    }

    int code;
    QoreStringNodeHolder mr(priv->sendMsg(code, cmd, arg, xsink));
    if (!mr) {
        assert(*xsink);
        return nullptr;
    }

    // remove code string from message
    assert(mr->find(' ') == 3);
    mr->splice(0, 4, xsink);
    assert(!*xsink);

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclFtpResponseInfo, xsink), xsink);
    rv->setKeyValue("code", code, xsink);
    rv->setKeyValue("msg", mr.release(), xsink);
    return rv.release();
}

// public locked
QoreStringNode* QoreFtpClient::list(const char* path, bool long_list, ExceptionSink* xsink) {
    SafeLocker sl(priv->m);
    if (priv->checkConnectedUnlocked(xsink))
        return nullptr;

    QoreStringNode* rv = priv->listAsyncBlocking(path, long_list, xsink);
    sl.unlock();
    return rv;
}

// public locked
int QoreFtpClient::put(const char* localpath, const char* remotename, ExceptionSink* xsink) {
    printd(5, "QoreFtpClient::put(%s, %s)\n", localpath, remotename ? remotename : "NULL");

    QoreSandboxManagerHelper smh;
    if (smh && !smh->checkFilesystemAccess(localpath, QSEC_READ, xsink)) {
        return -1;
    }

    SafeLocker sl(priv->m);
    if (priv->checkConnectedUnlocked(xsink))
        return -1;

    // Read file into memory and use async put
    TmpLocalName ln(remotename, localpath);
    int fd = open(localpath, O_RDONLY, 0);
    if (fd < 0) {
        xsink->raiseErrnoException("FTP-FILE-OPEN-ERROR", errno, "%s", localpath);
        return -1;
    }

    struct stat file_info;
    if (fstat(fd, &file_info) == -1) {
        close(fd);
        xsink->raiseErrnoException("FTP-FILE-PUT-ERROR", errno, "could not get file size");
        return -1;
    }

    SimpleRefHolder<BinaryNode> data(new BinaryNode);
    if (file_info.st_size > 0) {
        data->preallocate(file_info.st_size);
    }
    char buf[65536];
    unsigned cancel_check = 0;
    while (true) {
        if (!(cancel_check++ % 100) && qore_check_cancel(xsink, "FTP file read")) {
            close(fd);
            return -1;
        }
        ssize_t nread = read(fd, buf, sizeof(buf));
        if (nread < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            xsink->raiseErrnoException("FTP-FILE-PUT-ERROR", errno, "error reading file");
            return -1;
        }
        if (!nread) {
            break;
        }
        data->append(buf, static_cast<size_t>(nread));
    }
    close(fd);
    int rv = priv->putAsyncBlocking((*data)->getPtr(), (*data)->size(), *ln, xsink);
    sl.unlock();
    return rv;
}

// public locked
int QoreFtpClient::put(InputStream *is, const char* remotename, ExceptionSink* xsink) {
    printd(5, "QoreFtpClient::put(InputStream, %s)\n", remotename ? remotename : "NULL");

    SafeLocker sl(priv->m);
    if (priv->checkConnectedUnlocked(xsink))
        return -1;

    // Read InputStream into memory for async dispatch.  Use is->read() directly
    // because PipeInputStream::read is internally synchronized and thread-safe.
    SimpleRefHolder<BinaryNode> buf(new BinaryNode);
    char chunk[65536];
    unsigned cancel_check = 0;
    while (true) {
        if (!(cancel_check++ % 100) && qore_check_cancel(xsink, "FTP input stream read")) {
            return -1;
        }
        int64_t nread = is->read(chunk, sizeof(chunk), xsink);
        if (*xsink) {
            return -1;
        }
        if (nread <= 0) {
            break;
        }
        buf->append(chunk, static_cast<size_t>(nread));
    }
    int rv = priv->putAsyncBlocking((*buf)->getPtr(), (*buf)->size(), remotename, xsink);
    sl.unlock();
    return rv;
}

// public locked
int QoreFtpClient::putData(const void *data, size_t len, const char* remotename, ExceptionSink* xsink) {
   assert(remotename);

   printd(5, "QoreFtpClient::putData(%p, %ld, %s)\n", data, len, remotename);

   SafeLocker sl(priv->m);
   if (priv->checkConnectedUnlocked(xsink))
      return -1;

   int rv = priv->putAsyncBlocking(data, len, remotename, xsink);
   sl.unlock();
   return rv;
}

// public locked
int QoreFtpClient::get(const char* remotepath, const char* localname, ExceptionSink* xsink) {
    printd(5, "QoreFtpClient::get(%s, %s)\n", remotepath, localname ? localname : "NULL");

    TmpLocalName ln(localname, remotepath);
    QoreSandboxManagerHelper smh;
    if (smh && !smh->checkFilesystemAccess(*ln, QSEC_WRITE | QSEC_CREATE, xsink)) {
        return -1;
    }

    SafeLocker sl(priv->m);
    if (priv->checkConnectedUnlocked(xsink)) {
        return -1;
    }

    SimpleRefHolder<BinaryNode> data(priv->getAsyncBlocking(remotepath, xsink));
    sl.unlock();
    if (*xsink) {
        return -1;
    }
    int fd = open(*ln, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        xsink->raiseErrnoException("FTP-FILE-OPEN-ERROR", errno, "%s", *ln);
        return -1;
    }
    if (*data && (*data)->size() > 0) {
        const char* ptr = static_cast<const char*>((*data)->getPtr());
        size_t remaining = (*data)->size();
        unsigned cancel_check = 0;
        while (remaining) {
            if (!(cancel_check++ % 100) && qore_check_cancel(xsink, "FTP file write")) {
                close(fd);
                return -1;
            }
            size_t chunk = remaining > 65536 ? 65536 : remaining;
            ssize_t nw = write(fd, ptr, chunk);
            if (nw < 0) {
                if (errno == EINTR) {
                    continue;
                }
                int en = errno;
                close(fd);
                xsink->raiseErrnoException("FTP-FILE-WRITE-ERROR", en, "error writing file");
                return -1;
            }
            if (!nw) {
                close(fd);
                xsink->raiseException("FTP-FILE-WRITE-ERROR", "short write while writing file");
                return -1;
            }
            ptr += nw;
            remaining -= static_cast<size_t>(nw);
        }
    }
    close(fd);
    return 0;
}

// public locked
int QoreFtpClient::get(const char* remotepath, OutputStream *os, ExceptionSink* xsink) {
   printd(5, "QoreFtpClient::get(%s, OutputStream)\n", remotepath);

   SafeLocker sl(priv->m);
   if (priv->checkConnectedUnlocked(xsink))
      return -1;

   SimpleRefHolder<BinaryNode> data(priv->getAsyncBlocking(remotepath, xsink));
   sl.unlock();
   if (*xsink) {
      return -1;
   }
   if (*data && (*data)->size() > 0) {
      os->write((*data)->getPtr(), (*data)->size(), xsink);
      if (*xsink) {
         return -1;
      }
   }
   return 0;
}

// public locked
QoreStringNode* QoreFtpClient::getAsString(const char* remotepath, ExceptionSink* xsink) {
    return getAsString(xsink, remotepath, QCS_DEFAULT);
}

QoreStringNode* QoreFtpClient::getAsString(ExceptionSink* xsink, const char* remotepath,
        const QoreEncoding* encoding) {
    printd(5, "QoreFtpClient::getAsString(%s)\n", remotepath);

    SafeLocker sl(priv->m);
    if (priv->checkConnectedUnlocked(xsink)) {
        return nullptr;
    }

    SimpleRefHolder<BinaryNode> data(priv->getAsyncBlocking(remotepath, xsink));
    sl.unlock();
    if (*xsink || !*data) {
        return nullptr;
    }
    return new QoreStringNode(static_cast<const char*>((*data)->getPtr()),
        (*data)->size(), encoding);
}

// public locked
BinaryNode* QoreFtpClient::getAsBinary(const char* remotepath, ExceptionSink* xsink) {
    printd(5, "QoreFtpClient::getAsBinary(%s)\n", remotepath);

    SafeLocker sl(priv->m);
    if (priv->checkConnectedUnlocked(xsink)) {
        return nullptr;
    }

    BinaryNode* rv = priv->getAsyncBlocking(remotepath, xsink);
    sl.unlock();
    return rv;
}

// public locked
int QoreFtpClient::rename(const char* from, const char* to, ExceptionSink* xsink) {
    SafeLocker sl(priv->m);
    if (priv->checkConnectedUnlocked(xsink))
        return -1;

    printd(FTPDEBUG, "QoreFtpClient::rename(from=%s, to=%s)\n", from, to);

    int code;
    FtpResp resp(priv->sendMsg(code, "RNFR", from, xsink));
    if (*xsink)
        return -1;

    if ((code / 100) != 3) {
        xsink->raiseException("FTP-RENAME-ERROR", "rename('%s' -> '%s'): server rejected original path: FTP server replied: %s", from, to, resp.c_str());
        return -1;
    }

    resp.assign(priv->sendMsg(code, "RNTO", to, xsink));
    if (*xsink)
        return -1;

    if ((code / 100) != 2) {
        xsink->raiseException("FTP-RENAME-ERROR", "rename('%s' -> '%s'): server rejected target path: FTP server replied: %s", from, to, resp.c_str());
        return -1;
    }

    return 0;
}

// public locked
int QoreFtpClient::cwd(const char* dir, ExceptionSink* xsink) {
    SafeLocker sl(priv->m);
    if (priv->checkConnectedUnlocked(xsink))
        return -1;

    int code;
    QoreStringNode* mr = priv->sendMsg(code, "CWD", dir, xsink);
    if (!mr) {
        assert(*xsink);
        return -1;
    }

    sl.unlock();

    QoreStringNodeHolder p(mr);
    if ((code / 100) == 2)
        return 0;

    p->chomp();
    xsink->raiseException("FTP-CWD-ERROR", "FTP server returned an error to the CWD command: %s", p->c_str());
    return -1;
}

static QoreStringNode* get_ftp_quoted_string(QoreStringNode* str) {
    // find leading quote
    qore_offset_t start = str->find('"');
    if (start >= 0) {
        ++start;
        qore_offset_t end = str->rfind('"');
        if (end > start) {
            int et = str->size() - end;
            str->replace(0, start, (const char*)0);
            str->replace(str->size() - et, et, (const char*)0);
            str->replaceAll("\"\"", "\"");
        }
    }

    return str;
}

// public locked
QoreStringNode* QoreFtpClient::pwd(ExceptionSink* xsink) {
    SafeLocker sl(priv->m);
    if (priv->checkConnectedUnlocked(xsink))
        return nullptr;

    int code;
    QoreStringNode* mr = priv->sendMsg(code, "PWD", 0, xsink);
    if (!mr) {
        assert(*xsink);
        return nullptr;
    }

    sl.unlock();

    QoreStringNodeHolder p(mr);
    if ((getFTPCode(*p) / 100) == 2) {
        QoreStringNode* rv = p->substr(4, xsink);
        assert(!*xsink); // not possible to have an exception here
        rv->chomp();
        return get_ftp_quoted_string(rv);
    }
    p->chomp();
    xsink->raiseException("FTP-PWD-ERROR", "FTP server returned an error response to the PWD command: %s", p->c_str());
    return nullptr;
}

// public locked
int QoreFtpClient::del(const char* file, ExceptionSink* xsink) {
    SafeLocker sl(priv->m);
    if (priv->checkConnectedUnlocked(xsink))
        return -1;

    int code;
    QoreStringNode* mr = priv->sendMsg(code, "DELE", file, xsink);
    if (!mr) {
        assert(*xsink);
        return -1;
    }

    sl.unlock();

    QoreStringNodeHolder p(mr);
    if ((code / 100) == 2)
        return 0;

    p->chomp();
    xsink->raiseException("FTP-DELETE-ERROR", "FTP server returned an error to the DELE command: %s", p->c_str());
    return -1;
}

int QoreFtpClient::mkdir(const char* remotepath, ExceptionSink* xsink) {
    SafeLocker sl(priv->m);
    if (priv->checkConnectedUnlocked(xsink))
        return -1;

    int code;
    QoreStringNode* mr = priv->sendMsg(code, "MKD", remotepath, xsink);
    if (!mr) {
        assert(*xsink);
        return -1;
    }

    sl.unlock();

    QoreStringNodeHolder p(mr);
    if ((code / 100) == 2)
        return 0;

    p->chomp();
    xsink->raiseException("FTP-MKDIR-ERROR", "FTP server returned an error to the MKD command: %s", p->c_str());
    return -1;
}

int QoreFtpClient::rmdir(const char* remotepath, ExceptionSink* xsink) {
    SafeLocker sl(priv->m);
    if (priv->checkConnectedUnlocked(xsink))
        return -1;

    int code;
    QoreStringNode* mr = priv->sendMsg(code, "RMD", remotepath, xsink);
    if (!mr) {
        assert(*xsink);
        return -1;
    }

    sl.unlock();

    QoreStringNodeHolder p(mr);
    if ((code / 100) == 2)
        return 0;

    p->chomp();
    xsink->raiseException("FTP-RMDIR-ERROR", "FTP server returned an error to the RMD command: %s", p->c_str());
    return -1;
}

void QoreFtpClient::setURL(const QoreString* url, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    priv->setURLIntern(url, xsink);
}

QoreStringNode* QoreFtpClient::getURL() const {
    AutoLocker al(priv->m);
    QoreStringNode* url = new QoreStringNode("ftp");
    if (priv->secure)
        url->concat('s');
    url->concat("://");
    if (priv->user) {
        url->concat(priv->user);
        if (priv->pass)
            url->sprintf(":%s", priv->pass);
        url->concat('@');
    }
    if (priv->host)
        url->concat(priv->host);
    if (priv->port)
        url->sprintf(":%d", priv->port);
    return url;
}

void QoreFtpClient::setPort(int p) {
    priv->port = p;
}

void QoreFtpClient::setUserName(const char* u) {
    AutoLocker al(priv->m);
    if (priv->user)
        free(priv->user);
    priv->user = u ? strdup(u) : 0;
}

void QoreFtpClient::setPassword(const char* p) {
    AutoLocker al(priv->m);
    if (priv->pass)
        free(priv->pass);
    priv->pass = p ? strdup(p) : 0;
}

void QoreFtpClient::setHostName(const char* h) {
    AutoLocker al(priv->m);
    if (priv->host)
        free(priv->host);
    priv->host = h ? strdup(h) : 0;
}

int QoreFtpClient::setSecure() {
    AutoLocker al(priv->m);
    if (priv->control_connected)
        return -1;
    priv->secure = priv->secure_data = true;
    return 0;
}

int QoreFtpClient::setInsecure() {
    AutoLocker al(priv->m);
    if (priv->control_connected)
        return -1;
    priv->secure = priv->secure_data = false;
    return 0;
}

int QoreFtpClient::setInsecureData() {
    AutoLocker al(priv->m);
    if (priv->control_connected)
        return -1;
    priv->secure_data = false;
    return 0;
}

// returns true if the control connection can only be established with a secure connection
bool QoreFtpClient::isSecure() const {
    return priv->secure;
}

// returns true if data connections can only be established with a secure connection
bool QoreFtpClient::isDataSecure() const {
    return priv->secure_data;
}

bool QoreFtpClient::isConnected() const {
    return priv->control_connected;
}

const char* QoreFtpClient::getSSLCipherName() const {
    return priv->getSSLCipherName();
}

const char* QoreFtpClient::getSSLCipherVersion() const {
    return priv->getSSLCipherVersion();
}

long QoreFtpClient::verifyPeerCertificate() const {
    return priv->verifyPeerCertificate();
}

void QoreFtpClient::setModeAuto() {
    AutoLocker al(priv->m);
    priv->mode = FTP_MODE_UNKNOWN;
    priv->manual_mode = false;
}

void QoreFtpClient::setModeEPSV() {
    AutoLocker al(priv->m);
    priv->mode = FTP_MODE_EPSV;
    priv->manual_mode = true;
}

void QoreFtpClient::setModePASV() {
    AutoLocker al(priv->m);
    priv->mode = FTP_MODE_PASV;
    priv->manual_mode = true;
}

void QoreFtpClient::setModePORT() {
    AutoLocker al(priv->m);
    priv->mode = FTP_MODE_PORT;
    priv->manual_mode = true;
}

const char* QoreFtpClient::getMode() const {
    switch (priv->mode) {
        case FTP_MODE_PORT: return "port";
        case FTP_MODE_PASV: return "pasv";
        case FTP_MODE_EPSV: return "epsv";
    }
    return "auto";
}

int QoreFtpClient::getPort() const {
    return priv->port;
}

const char* QoreFtpClient::getUserName() const {
    return priv->user;
}

const char* QoreFtpClient::getPassword() const {
    return priv->pass;
}

const char* QoreFtpClient::getHostName() const {
    return priv->host;
}

void QoreFtpClient::setEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
    priv->setEventQueue(xsink, q, arg, with_data);
}

void QoreFtpClient::setControlEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
    priv->setControlEventQueue(xsink, q, arg, with_data);
}

void QoreFtpClient::setDataEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
    priv->setDataEventQueue(xsink, q, arg, with_data);
}

void QoreFtpClient::cleanup(ExceptionSink* xsink) {
    priv->cleanup(xsink);
}

void QoreFtpClient::clearWarningQueue(ExceptionSink* xsink) {
    priv->clearWarningQueue(xsink);
}

void QoreFtpClient::setWarningQueue(ExceptionSink* xsink, int64 warning_ms, int64 warning_bs, Queue* wq,
        QoreValue arg, int64 min_ms) {
    priv->setWarningQueue(xsink, warning_ms, warning_bs, wq, arg, min_ms);
}

QoreHashNode* QoreFtpClient::getUsageInfo() const {
    return priv->getUsageInfo();
}

void QoreFtpClient::clearStats() {
    priv->clearStats();
}

void QoreFtpClient::setTimeout(int timeout_ms) {
    priv->timeout_ms = timeout_ms;
}

int QoreFtpClient::getTimeout() const {
    return priv->timeout_ms;
}

void QoreFtpClient::setNetworkFamily(int family) {
    priv->setNetworkFamily(family);
}

int QoreFtpClient::getNetworkFamily() const {
    return priv->getNetworkFamily();
}

QoreHashNode* QoreFtpClient::getControlPeerInfo(ExceptionSink* xsink, bool host_lookup) const {
    return priv->getControlPeerInfo(xsink, host_lookup);
}

QoreHashNode* QoreFtpClient::getDataPeerInfo(ExceptionSink* xsink, bool host_lookup) const {
    return priv->getDataPeerInfo(xsink, host_lookup);
}

QoreHashNode* QoreFtpClient::getControlSocketInfo(ExceptionSink* xsink, bool host_lookup) const {
    return priv->getControlSocketInfo(xsink, host_lookup);
}

QoreHashNode* QoreFtpClient::getDataSocketInfo(ExceptionSink* xsink, bool host_lookup) const {
    return priv->getDataSocketInfo(xsink, host_lookup);
}

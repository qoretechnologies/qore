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
#include "qore/intern/qore_socket_private.h"
#include "qore/intern/qore_string_private.h"

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
    // for when we read too much data on control connection
    QoreString buffer;
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
        manual_mode = false,
        use_async = true;  //!< dispatch through async I/O controller

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
    }

    //! Clean up async control op (must be called with ExceptionSink before destructor)
    DLLLOCAL void cleanupAsync(ExceptionSink* xsink) {
        if (ctrl_op_obj) {
            // Cancel our operations from the async controller cache before deref
            ReferenceHolder<QoreObject> ctl_obj(qore_get_async_io_controller_obj(xsink), xsink);
            if (!*xsink && *ctl_obj) {
                ReferenceHolder<AsyncIoControllerPriv> ctl_priv(
                    static_cast<AsyncIoControllerPriv*>(
                        (*ctl_obj)->getReferencedPrivateData(CID_ASYNCIOCONTROLLER, xsink)),
                    xsink);
                if (!*xsink && *ctl_priv) {
                    SimpleRefHolder<QoreStringNode> owner(new QoreStringNode(async_owner));
                    ctl_priv->cancelByOwner(*owner, xsink);
                }
            }
            if (*xsink) {
                xsink->clear();
            }
            ctrl_op = nullptr;
            ctrl_op_obj->deref(xsink);
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

    DLLLOCAL void setControlEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
        AutoLocker al(m);
        control.setEventQueue(xsink, q, arg, with_data);
    }

    DLLLOCAL void setDataEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
        AutoLocker al(m);
        data.setEventQueue(xsink, q, arg, with_data);
    }

    DLLLOCAL void setEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
        AutoLocker al(m);
        control.setEventQueue(xsink, q, arg, with_data);
        if (q)
            q->ref();
        data.setEventQueue(xsink, q, arg, with_data);
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

    DLLLOCAL void do_event_send_msg(const char* cmd, const char* arg) {
        Queue *q = control.getQueue();
        if (q) {
            QoreHashNode* h = qore_socket_private::get(control)->getEvent(QORE_EVENT_FTP_SEND_MESSAGE, QORE_SOURCE_FTPCLIENT);
            h->setKeyValue("command", new QoreStringNode(cmd), nullptr);
            if (arg)
                h->setKeyValue("arg", new QoreStringNode(arg), nullptr);
            q->pushAndTakeRef(h);
        }
    }

    DLLLOCAL void do_event_msg_received(int code, const char* msg) {
        Queue *q = control.getQueue();
        if (q) {
            QoreHashNode* h = qore_socket_private::get(control)->getEvent(QORE_EVENT_FTP_MESSAGE_RECEIVED, QORE_SOURCE_FTPCLIENT);
            h->setKeyValue("code", code, nullptr);
            h->setKeyValue("message", msg[0] ? QoreValue(new QoreStringNode(msg)) : QoreValue(), nullptr);
            q->pushAndTakeRef(h);
        }
    }

    // unlocked
    DLLLOCAL int checkConnectedUnlocked(ExceptionSink* xsink) {
        // Single chokepoint — every public sync FtpClient entry point routes
        // through this helper before touching the control/data sockets, so
        // one assert here catches any I/O-thread misuse at the FtpClient API
        // level with a clear class name in the error message.
        SocketSyncPoll::assertNotOnIoThread("FtpClient", "ftp", xsink);
        return (!loggedin || !control.isOpen()) && connectUnlocked(xsink) ? -1 : 0;
    }

    DLLLOCAL void disconnectIntern() {
        control.close();
        control_connected = false;
        if (!manual_mode) {
            mode = FTP_MODE_UNKNOWN;
        }
        data.close();
        loggedin = false;
        // Clear async pointers — don't cancel from controller here;
        // cleanupAsync() handles that during object destruction
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
    DLLLOCAL QoreStringNode* getResponse(int &code, ExceptionSink* xsink) {
        // Use async recv if available
        if (use_async && ctrl_op) {
            return recvResponseAsyncBlocking(code, xsink);
        }
        QoreStringNodeHolder resp(nullptr);
        // if there is data in the buffer, then take it, otherwise read
        if (!buffer.strlen()) {
            resp = control.recv(timeout_ms, xsink);
            if (*xsink) {
                disconnectIntern();
                return nullptr;
            }
        } else {
            size_t len = buffer.strlen();
            resp = new QoreStringNode(buffer.giveBuffer(), len, len + 1, buffer.getEncoding());
        }
        // see if we got the whole response
        if (resp && resp->c_str()) {
            const char* start = resp->c_str();
            const char* p = start;
            while (true) {
                if ((*p) == '\n') {
                    if (p > (start + 3)) {
                        // if we got the whole response
                        if (isdigit(*start) && isdigit(start[1]) && isdigit(start[2]) && start[3] == ' ') {
                            code = ((*start - 48) * 100) + ((start[1] - 48) * 10) + start[2] - 48;
                            // if we read more data, then store it in the buffer
                            if (p[1] != '\0') {
                                buffer.set(&p[1]);
                                resp->terminate(p - resp->c_str() + 1);
                            }
                            break;
                        }
                    }
                    start = p + 1;
                } else if (*p == '\0') {
                    // if we have not got the whole message
                    QoreStringNodeHolder r(control.recv(timeout_ms, xsink));
                    if (*xsink) {
                        disconnectIntern();
                        return nullptr;
                    }
                    if (!r) {
                        disconnectIntern();
                        xsink->raiseException("FTP-RECEIVE-ERROR", "short message received on control port");
                        return nullptr;
                    }
                    //printd(FTPDEBUG, "QoreFtpClient::getResponse() read %s\n", r->c_str());
                    // in case the buffer gets reallocated
                    int pos = p - resp->c_str();
                    // cannot maintain start across buffer reallocations
                    size_t offset = p - start;
                    qore_string_private::get(*resp)->concat(r);
                    p = resp->c_str() + pos;
                    start = p + offset;
                }
                p++;
            }
        }
        printd(FTPDEBUG, "QoreFtpClient::getResponse() %s", resp ? resp->c_str() : "NULL");
        if (resp) {
            resp->chomp();
            do_event_msg_received(code, resp->c_str() + 4);
        } else {
            disconnectIntern();
            xsink->raiseException("FTP-RECEIVE-ERROR", "FTP server sent an empty response on the control port");
        }
        return resp.release();
    }

    // unlocked
    DLLLOCAL int connectIntern(FtpResp *resp, ExceptionSink* xsink) {
        // connect to FTP port on remote machine
        QoreStringMaker portstr("%d", port);
        if (control.connectINET2(host, portstr.c_str(), q_get_raf(family), Q_SOCK_STREAM, 0, timeout_ms, xsink)) {
            return -1;
        }

        control_connected = 1;

        int code;
        resp->assign(getResponse(code, xsink));

        if (*xsink)
            return -1;

        printd(FTPDEBUG, "qore_ftp_private::connectIntern() %s", resp->c_str());

        // ex: 220 (vsFTPd 2.0.1)
        // ex: 220 localhost FTP server (tnftpd 20040810) ready.
        // etc
        if ((code / 100) != 2) {
            xsink->raiseException("FTP-CONNECT-ERROR", "FTP server reported the following error: %s", resp->c_str());
            return -1;
        }

        return 0;
    }

    // unlocked
    DLLLOCAL QoreStringNode* sendMsg(int &code, const char* cmd, const char* arg, ExceptionSink* xsink) {
        if (use_async && ctrl_op) {
            return sendMsgAsyncBlocking(code, cmd, arg, xsink);
        }

        do_event_send_msg(cmd, arg);

        QoreString c(cmd);
        if (arg) {
            c.concat(' ');
            c.concat(arg);
        }
        c.concat("\r\n");
        printd(FTPDEBUG, "QoreFtpClient::sendMsg() %s", c.c_str());
        if (control.send(c.c_str(), c.strlen(), timeout_ms, xsink) < 0) {
            disconnectIntern();
            if (!*xsink)
                xsink->raiseException("FTP-SEND-ERROR", q_strerror(errno));
            return 0;
        }

        QoreStringNode* rsp = getResponse(code, xsink);
        return rsp;
    }

    // do PBSZ and PROT commands
    DLLLOCAL int doProt(FtpResp *resp, ExceptionSink* xsink) {
        int code;
        // RFC-4217: PBSZ 0 for streaming data

        QoreStringNode* mr = sendMsg(code, "PBSZ", "0", xsink);
        if (!mr) {
            assert(*xsink);
            return -1;
        }

        resp->assign(mr);
        if (code != 200) {
            xsink->raiseException("FTPS-SECURE-DATA-ERROR", "response from FTP server to PBSZ 0 command: %s", resp->c_str());
            return -1;
        }

        mr = sendMsg(code, "PROT", "P", xsink);
        if (!mr) {
            assert(*xsink);
            return -1;
        }

        resp->assign(mr);
        if (code != 200) {
            xsink->raiseException("FTPS-SECURE-DATA-ERROR", "response from FTP server to PROT P command: %s", resp->c_str());
            return -1;
        }

        return 0;
    }

    // unlocked
    DLLLOCAL int doAuth(FtpResp *resp, ExceptionSink* xsink) {
        int code;

        QoreStringNode* mr = sendMsg(code, "AUTH", "TLS", xsink);
        if (!mr) {
            assert(*xsink);
            return -1;
        }
        resp->assign(mr);

        if (code != 234) {
            // RFC-2228 ADAT exchange not supported
            if (code == 334)
                xsink->raiseException("FTPS-AUTH-ERROR", "server requires unsupported ADAT exchange");
            else {
                xsink->raiseException("FTPS-AUTH-ERROR", "response from FTP server: %s", resp->c_str());
            }
            return -1;
        }

        if (control.upgradeClientToSSL(xsink, timeout_ms))
            return -1;

        if (secure_data)
            return doProt(resp, xsink);

        return 0;
    }

    //! Async connect: creates FtpControlPollOperation, submits to controller, blocks
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

        QoreStringMaker target("%s:%d", host, port);
        sock_priv->ref();  // ref for connect op
        SocketConnectPollOperation* connect_op = new SocketConnectPollOperation(xsink,
            false, target.c_str(), sock_priv);
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

        // Submit to async I/O controller
        if (ctrl_op->submitToController(xsink, async_owner.c_str())) {
            cleanupAsync(xsink);
            return -1;
        }

        // Block until login completes
        if (ctrl_op->waitForCompletion(timeout_ms, xsink)) {
            cleanupAsync(xsink);
            return -1;
        }

        // Check result
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

        // Re-submit to controller for the command
        if (ctrl_op->submitToController(xsink, async_owner.c_str())) {
            return nullptr;
        }

        // Block until command response
        if (ctrl_op->waitForCompletion(timeout_ms, xsink)) {
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
        if (ctrl_op->submitToController(xsink, async_owner.c_str())) {
            return nullptr;
        }
        if (ctrl_op->waitForCompletion(timeout_ms, xsink)) {
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
    /** Returns the data op priv (for waitForCompletion) and the QoreObject (for cleanup).
        Does NOT block — the caller must send the transfer command (RETR/STOR/LIST) on
        the control channel and then call data_op->waitForCompletion().
    */
    DLLLOCAL FtpDataPollOperationPriv* createAndSubmitDataOp(
            const char* data_host, int data_port, bool recv_mode,
            BinaryNode* send_data, QoreObject*& data_op_obj_out,
            ExceptionSink* xsink) {
        // Create data socket.
        // Refcount plan: new=1 (consumed below by `new QoreObject(QC_SOCKET...)`
        // which takes ownership of the priv); +1 ref here for the connect op.
        QoreSocketObject* dsock = new QoreSocketObject;

        QoreStringMaker dtarget("%s:%d", data_host, data_port);
        dsock->ref();  // for connect op
        SocketConnectPollOperation* dconnect = new SocketConnectPollOperation(xsink,
            false, dtarget.c_str(), dsock);
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

        // Submit to controller (non-blocking — starts connect immediately)
        if (dop->submitToController(xsink, async_owner.c_str())) {
            dop_obj->deref(xsink);
            return nullptr;
        }

        data_op_obj_out = dop_obj;
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
        if (accept_priv->submitToController(xsink, async_owner.c_str())) {
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
        if (accept_priv->waitForCompletion(timeout_ms, xsink)) {
            return -1;
        }

        // 7. Get accepted socket
        ValueHolder accepted_val(accept_priv->getOutput(), xsink);
        if (accepted_val->getType() != NT_OBJECT) {
            xsink->raiseException("FTP-CONNECT-ERROR", "PORT accept did not return a socket");
            return -1;
        }
        QoreObject* accepted_obj = accepted_val->get<QoreObject>();
        QoreSocketObject* accepted_sock = static_cast<QoreSocketObject*>(
            accepted_obj->getReferencedPrivateData(CID_SOCKET, xsink));
        if (*xsink || !accepted_sock) {
            if (!*xsink) {
                xsink->raiseException("FTP-CONNECT-ERROR", "failed to get accepted socket");
            }
            return -1;
        }

        // 8. Create FtpDataPollOperation with adopt-socket constructor
        accepted_sock->ref();  // for QoreObject setPrivate
        QoreObject* data_op_obj = new QoreObject(QC_FTPDATAPOLLOPERATION, getProgram());
        accepted_obj->ref();
        data_op_obj->setMemberValue("sock", QC_FTPDATAPOLLOPERATION, accepted_obj, xsink);
        data_op_obj->setMemberValue("goal", QC_FTPDATAPOLLOPERATION,
            new QoreStringNode(recv_output ? "ftp-data-recv" : "ftp-data-send"), xsink);
        if (*xsink) {
            accepted_sock->deref(xsink);
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
        if (data_op->submitToController(xsink, async_owner.c_str())) {
            return -1;
        }
        if (data_op->waitForCompletion(timeout_ms, xsink)) {
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
        FtpDataPollOperationPriv* data_op = createAndSubmitDataOp(
            dhost.c_str(), dport, true, nullptr, data_op_obj, xsink);
        if (!data_op) {
            return nullptr;
        }
        ReferenceHolder<QoreObject> data_op_holder(data_op_obj, xsink);

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
        if (data_op->waitForCompletion(timeout_ms, xsink)) {
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
        FtpDataPollOperationPriv* data_op = createAndSubmitDataOp(
            dhost.c_str(), dport, false, send_bin, data_op_obj, xsink);
        if (!data_op) {
            send_bin->deref();
            return -1;
        }
        ReferenceHolder<QoreObject> data_op_holder(data_op_obj, xsink);

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
        if (data_op->waitForCompletion(timeout_ms, xsink)) {
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
        FtpDataPollOperationPriv* data_op = createAndSubmitDataOp(
            dhost.c_str(), dport, true, nullptr, data_op_obj, xsink);
        if (!data_op) {
            return nullptr;
        }
        ReferenceHolder<QoreObject> data_op_holder(data_op_obj, xsink);

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
        if (data_op->waitForCompletion(timeout_ms, xsink)) {
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
        if (use_async) {
            return connectAsyncBlocking(xsink);
        }

        disconnectIntern();

        if (!host) {
            xsink->raiseException("FTP-CONNECT-ERROR", "no hostname set");
            return -1;
        }

        FtpResp resp;
        if (connectIntern(&resp, xsink)) {
            assert(*xsink);
            return -1;
        }

        if (secure && doAuth(&resp, xsink)) {
            assert(*xsink);
            return -1;
        }

        int code;

        QoreStringNode* mr = sendMsg(code, "USER", user ? user : (char* )DEFAULT_USERNAME, xsink);
        if (!mr) {
            assert(*xsink);
            return -1;
        }

        resp.assign(mr);

        // if user not logged in immediately, continue
        if ((code / 100) != 2) {
            // if there is an error, then exit
            if (code != 331) {
                xsink->raiseException("FTP-LOGIN-ERROR", "response from FTP server: %s", resp.c_str());
                return -1;
            }

            // send password
            mr = sendMsg(code, "PASS", pass ? pass : (char* )DEFAULT_PASSWORD, xsink);
            if (!mr) {
                assert(*xsink);
                return -1;
            }

            resp.assign(mr);

            // if user not logged in for whatever reason, then exit
            if ((code / 100) != 2) {
                xsink->raiseException("FTP-LOGIN-ERROR", "response from FTP server: %s", resp.c_str());
                return -1;
            }
        }

        loggedin = true;

        return 0;
    }

    // unlocked
    DLLLOCAL int setBinaryMode(bool t, ExceptionSink* xsink) {
        // set transfer mode
        int code;
        QoreStringNode* mr = sendMsg(code, "TYPE", (char* )(t ? "I" : "A"), xsink);
        if (!mr) {
            assert(*xsink);
            return -1;
        }

        QoreStringNodeHolder resp(mr);

        if ((code / 100) != 2) {
            xsink->raiseException("FTP-ERROR", "can't set mode to '%c', FTP server responded: %s", (t ? 'I' : 'A'), resp->c_str());
            return -1;
        }
        return 0;
    }

    // unlocked
    DLLLOCAL int acceptDataConnection(ExceptionSink* xsink) {
        // issue #3031: make sure and use a timeout!
        if (data.acceptAndReplace(timeout_ms, xsink)) {
            data.close();
            if (!*xsink) {
                xsink->raiseErrnoException("FTP-CONNECT-ERROR", errno, "error accepting data connection");
            }
            return -1;
        }
#ifdef DEBUG
        if (secure_data)
            printd(FTPDEBUG, "QoreFtpClient::acceptDataConnection() negotiating client SSL connection\n");
#endif

        if (secure_data && data.upgradeClientToSSL(xsink, timeout_ms))
            return -1;

        printd(FTPDEBUG, "QoreFtpClient::acceptDataConnection() accepted PORT data connection\n");
        return 0;
    }

    // unlocked
    DLLLOCAL int connectData(ExceptionSink* xsink) {
        switch (mode) {
            case FTP_MODE_UNKNOWN:
                if (!connectDataExtendedPassive(xsink))
                    return 0;
                if (*xsink)
                    return -1;
                if (!connectDataPassive(xsink))
                    return 0;
                if (*xsink)
                    return -1;
                if (!connectDataPort(xsink))
                    return 0;

                if (!*xsink)
                    xsink->raiseException("FTP-CONNECT-ERROR", "Could not negotiate data channel connection with FTP server");
                return -1;
            case FTP_MODE_EPSV:
                return connectDataExtendedPassive(xsink);
            case FTP_MODE_PASV:
                return connectDataPassive(xsink);
            case FTP_MODE_PORT:
                return connectDataPort(xsink);
        }
        return -1;
    }

    // unlocked
    // RFC 2428 Extended Passive Mode
    DLLLOCAL int connectDataExtendedPassive(ExceptionSink* xsink) {
        // try extended passive mode
        int code;
        QoreStringNode* mr = sendMsg(code, "EPSV", 0, xsink);
        if (!mr) {
            assert(*xsink);
            return -1;
        }

        FtpResp resp(mr);
        if ((code / 100) != 2)
            return -1;

        // ex: 229 Entering Extended Passive Mode (|||63519|)
        // get port for data connection
        printd(FTPDEBUG, "EPSV: %s\n", resp.c_str());

        const char* s = strstr(resp.c_str(), "|||");
        if (!s) {
            xsink->raiseException("FTP-RESPONSE-ERROR", "cannot find port in EPSV response: %s", resp.c_str());
            return -1;
        }
        s += 3;
        char* end = (char* )strchr(s, '|');
        if (!end) {
            xsink->raiseException("FTP-RESPONSE-ERROR", "cannot find port in EPSV response: %s", resp.c_str());
            return -1;
        }
        *end = '\0';

        int data_port = atoi(s);
        if (data.connectINET2(host, s, q_get_raf(family), Q_SOCK_STREAM, 0, timeout_ms, xsink)) {
            if (!*xsink)
                xsink->raiseErrnoException("FTP-CONNECT-ERROR", errno, "could not connect to extended passive data port (%s:%d)", host, data_port);
            return -1;
        }
        printd(FTPDEBUG, "EPSV connected to %s:%d (open: %d family: %d)\n", host, data_port, data.isOpen(),
            qore_socket_private::get(data)->sfamily);

        mode = FTP_MODE_EPSV;
        return 0;
    }

    // unlocked
    DLLLOCAL int connectDataPassive(ExceptionSink* xsink) {
        // try passive mode
        int code;
        QoreStringNode* mr = sendMsg(code, "PASV", 0, xsink);
        if (!mr) {
            assert(*xsink);
            return -1;
        }

        FtpResp resp(mr);
        if ((code / 100) != 2) {
            return -1;
        }

        // reply ex: 227 Entering passive mode (127,0,0,1,28,46)
        // get port for data connection
        const char* s = strstr(resp.c_str(), "(");
        if (!s) {
            xsink->raiseException("FTP-RESPONSE-ERROR", "cannot parse PASV response: %s", resp.c_str());
            return -1;
        }
        int num[5];
        s++;
        const char* comma;
        for (int i = 0; i < 5; i++) {
            comma = strchr(s, ',');
            if (!comma) {
                xsink->raiseException("FTP-RESPONSE-ERROR", "cannot parse PASV response: %s", resp.c_str());
                return -1;
            }
            num[i] = atoi(s);
            s = comma + 1;
        }
        int dataport = (num[4] << 8) + atoi(s);
        QoreStringMaker ip("%d.%d.%d.%d", num[0], num[1], num[2], num[3]);
        printd(FTPDEBUG,"qore_ftp_private::connectPassive() address: %s:%d\n", ip.c_str(), dataport);
        QoreStringMaker port("%d", dataport);

        // issue #3031: PASV is only supported with IPv4
        if (data.connectINET2(ip.c_str(), port.c_str(), Q_AF_INET, Q_SOCK_STREAM, 0, timeout_ms, xsink)) {
            if (!*xsink) {
                xsink->raiseErrnoException("FTP-CONNECT-ERROR", errno, "could not connect to passive data port (%s:%d)", ip.c_str(), dataport);
            }
            return -1;
        }

        if (secure_data && data.upgradeClientToSSL(xsink, timeout_ms)) {
            return -1;
        }

        mode = FTP_MODE_PASV;
        return 0;
    }

    // unlocked
    DLLLOCAL int connectDataPort(ExceptionSink* xsink) {
        // get address for interface of control connection
        struct sockaddr_in add;
        socklen_t socksize = sizeof(struct sockaddr_in);

        // Use the async control socket if available, otherwise the legacy socket
        int ctrl_fd = (ctrl_op && ctrl_op->getControlSocket())
            ? ctrl_op->getControlSocket()->getSocket()
            : control.getSocket();
        if (getsockname(ctrl_fd, (struct sockaddr *)&add, &socksize) < 0) {
            xsink->raiseErrnoException("FTP-CONNECT-ERROR", errno, "cannot determine local interface address for data port connection");
            return -1;
        }
        // bind to any port on local interface
        // issue #3031: PORT is only supported with IPv4
        add.sin_family = AF_INET;
        add.sin_port = 0;
        if (data.bind((struct sockaddr *)&add, sizeof (struct sockaddr_in))) {
            xsink->raiseErrnoException("FTP-CONNECT-ERROR", errno, "could not bind to any port on local interface");
            return -1;
        }
        // get port number
        int dataport = data.getPort();

        // get ip address
        char ifname[80];
        if (!inet_ntop(AF_INET, &((struct sockaddr_in *)&add)->sin_addr, ifname, sizeof(ifname))) {
            data.close();
            xsink->raiseErrnoException("FTP-CONNECT-ERROR", errno, "cannot determine local interface address for data port connection");
            return -1;
        }
        printd(FTPDEBUG, "qore_ftp_private::connectDataPort() requesting connection to %s:%d\n", ifname, dataport);
        // change dots to commas for PORT message
        for (int i = 0; ifname[i]; i++)
            if (ifname[i] == '.')
                ifname[i] = ',';

        QoreString pconn;
        pconn.sprintf("%s,%d,%d", ifname, dataport >> 8, dataport & 255);
        int code;

        QoreStringNode* mr = sendMsg(code, "PORT", pconn.c_str(), xsink);
        if (!mr) {
            assert(*xsink);
            data.close();
            return -1;
        }

        FtpResp resp(mr);
        // ex: 200 PORT command successful.
        if ((code / 100) != 2) {
            data.close();
            return -1;
        }

        if (data.listen()) {
            int en = errno;
            data.close();
            xsink->raiseErrnoException("FTP-CONNECT-ERROR", en, "error listening on data connection");
            return -1;
        }
        printd(FTPDEBUG, "qore_ftp_private::connectDataPort() listening on port %d\n", dataport);

        mode = FTP_MODE_PORT;
        return 0;
    }

    // sets up a data connection and requests to retrieve a file
    // returns -1=error, 0=OK
    // private unlocked
    DLLLOCAL int pre_get(FtpResp &resp, const char* remotepath, ExceptionSink* xsink) {
        // set binary mode and establish data connection
        if (setBinaryMode(true, xsink) || connectData(xsink))
            return -1;

        // setup the file transfer on the data channel
        int code;

        QoreStringNode* mr = sendMsg(code, "RETR", remotepath, xsink);
        if (!mr) {
            assert(*xsink);
            data.close();
            return -1;
        }

        resp.assign(mr);
        //printf("%s", resp->c_str());

        if ((code / 100) != 1) {
            data.close();
            xsink->raiseException("FTP-GET-ERROR", "could not retrieve file, FTP server replied: %s", resp.c_str());
            return -1;
        }

        if ((mode == FTP_MODE_PORT && acceptDataConnection(xsink)) || *xsink) {
            data.close();
            return -1;
        }
        else if (secure_data && data.upgradeClientToSSL(xsink, timeout_ms)) {
            data.close();
            return -1;
        }

        return 0;
    }

    DLLLOCAL void disconnect() {
        m.lock();
        disconnectIntern();
        m.unlock();
    }

    DLLLOCAL void clearWarningQueue(ExceptionSink* xsink) {
        AutoLocker al(m);
        control.clearWarningQueue(xsink);
        data.clearWarningQueue(xsink);
    }

    DLLLOCAL void setWarningQueue(ExceptionSink* xsink, int64 warning_ms, int64 warning_bs, Queue* wq, QoreValue arg, int64 min_ms) {
        AutoLocker al(m);
        control.setWarningQueue(xsink, warning_ms, warning_bs, wq, arg, min_ms);
        if (!*xsink) {
            wq->ref();
            data.setWarningQueue(xsink, warning_ms, warning_bs, wq, arg.refSelf(), min_ms);
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

    if (priv->use_async && priv->ctrl_op) {
        QoreStringNode* rv = priv->listAsyncBlocking(path, long_list, xsink);
        sl.unlock();
        return rv;
    }

    if (priv->setBinaryMode(false, xsink) || priv->connectData(xsink))
        return nullptr;

    int code;
    QoreStringNode* mr = priv->sendMsg(code, (long_list ? "LIST" : "NLST"), path, xsink);
    if (!mr) {
        assert(*xsink);
        priv->data.close();
        return nullptr;
    }

    FtpResp resp(mr);

    //printd(5, "LIST cmd 0: %s\n", resp.c_str());
    // file not found or similar
    if ((code / 100 == 5)) {
        priv->data.close();
        return nullptr;
    }

    if ((code / 100 != 1)) {
        priv->data.close();
        xsink->raiseException("FTP-LIST-ERROR", "FTP server returned an error to the %s command: %s",
                                (long_list ? "LIST" : "NLST"), resp.c_str());
        return nullptr;
    }

    if ((priv->mode == FTP_MODE_PORT && priv->acceptDataConnection(xsink)) || *xsink) {
        priv->data.close();
        return nullptr;
    } else if (priv->secure_data && priv->data.upgradeClientToSSL(xsink, priv->timeout_ms))
        return nullptr;

    QoreStringNodeHolder l(new QoreStringNode);

    // read until done
    while (true) {
        int rc;
        if (!resp.assign(priv->data.recv(priv->timeout_ms, &rc))) {
            //printd(5, "read 0: ERR rc=%d l=%s\n", rc, l->c_str());
            break;
        }
        //printd(5, "read 0: rc=%d: resp=%s l=%s\n", rc, resp.c_str(), l->c_str());
        qore_string_private::get(*l)->concat(resp.getStr());
    }
    priv->data.close();
    resp.assign(priv->getResponse(code, xsink));
    sl.unlock();
    if (*xsink)
        return nullptr;

    printd(5, "read done: code=%d LIST: %s\n", code, resp.c_str());
    if ((code / 100 != 2)) {
        xsink->raiseException("FTP-LIST-ERROR", "FTP server returned an error to the %s command: %s",
                                (long_list ? "LIST" : "NLST"), resp.c_str());
        return nullptr;
    }
    return l.release();
}

// public locked
int QoreFtpClient::put(const char* localpath, const char* remotename, ExceptionSink* xsink) {
    printd(5, "QoreFtpClient::put(%s, %s)\n", localpath, remotename ? remotename : "NULL");

    SafeLocker sl(priv->m);
    if (priv->checkConnectedUnlocked(xsink))
        return -1;

    if (priv->use_async && priv->ctrl_op) {
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
            ssize_t nread = read(fd, const_cast<void*>(data->getPtr()), file_info.st_size);
            if (nread < 0) {
                close(fd);
                xsink->raiseErrnoException("FTP-FILE-PUT-ERROR", errno, "error reading file");
                return -1;
            }
            data->setSize(nread);
        }
        close(fd);
        int rv = priv->putAsyncBlocking((*data)->getPtr(), (*data)->size(), *ln, xsink);
        sl.unlock();
        return rv;
    }

    int fd = open(localpath, O_RDONLY, 0);
    if (fd < 0) {
        xsink->raiseErrnoException("FTP-FILE-OPEN-ERROR", errno, "%s", localpath);
        return -1;
    }
    ON_BLOCK_EXIT(close, fd);

    // set binary mode and establish data connection
    if (priv->setBinaryMode(true, xsink) || priv->connectData(xsink)) {
        return -1;
    }

    // get file size
    struct stat file_info;
    if (fstat(fd, &file_info) == -1) {
        int en = errno;
        xsink->raiseErrnoException("FTP-FILE-PUT-ERROR", en, "could not get file size");
        return -1;
    }

    // get remote file name
    TmpLocalName rn(remotename, localpath);
    // transfer file
    int code;
    QoreStringNode* mr = priv->sendMsg(code, "STOR", *rn, xsink);
    rn.discard();
    if (!mr) {
        assert(*xsink);
        priv->data.close();
        return -1;
    }

    FtpResp resp(mr);
    if (*xsink) {
        priv->data.close();
        return -1;
    }
    //printf("%s", resp->c_str());

    if ((code / 100) != 1) {
        priv->data.close();
        xsink->raiseException("FTP-PUT-ERROR", "could not put file, FTP server replied: %s", resp.c_str());
        return -1;
    }

    if ((priv->mode == FTP_MODE_PORT && priv->acceptDataConnection(xsink)) || *xsink) {
        priv->data.close();
        return -1;
    } else if (priv->secure_data && priv->data.upgradeClientToSSL(xsink, priv->timeout_ms)) {
        return -1;
    }

    int rc = priv->data.send(fd, file_info.st_size ? file_info.st_size : -1, priv->timeout_ms, xsink);
    priv->data.close();

    resp.assign(priv->getResponse(code, xsink));
    sl.unlock();
    if (*xsink)
        return -1;

    //printf("PUT: %s", resp->c_str());
    if ((code / 100 != 2)) {
        xsink->raiseException("FTP-PUT-ERROR", "FTP server returned an error to the STOR command: %s", resp.c_str());
        return -1;
    }

    if (rc) {
        xsink->raiseException("FTP-PUT-ERROR", "error sending file, may not be complete on target");
        return -1;
    }
    return 0;
}

// public locked
int QoreFtpClient::put(InputStream *is, const char* remotename, ExceptionSink* xsink) {
    printd(5, "QoreFtpClient::put(InputStream, %s)\n", remotename ? remotename : "NULL");

    SafeLocker sl(priv->m);
    if (priv->checkConnectedUnlocked(xsink))
        return -1;

    if (priv->use_async && priv->ctrl_op) {
        // Read InputStream into memory for async dispatch.
        // Use is->read() directly (not readHelper and without the
        // reassign/unassign thread dance): callers commonly wire a
        // StreamPipe in a producer/consumer pattern where the pipe's
        // InputStream was constructed in a different thread from the
        // one executing this put().  readHelper() / reassignThread()
        // both reject that; PipeInputStream::read itself is internally
        // synchronized and thread-safe, matching the sync path at
        // sendFromInputStream() below.
        SimpleRefHolder<BinaryNode> buf(new BinaryNode);
        char chunk[65536];
        while (true) {
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

    // set binary mode and establish data connection
    if (priv->setBinaryMode(true, xsink) || priv->connectData(xsink)) {
        return -1;
    }

    // transfer file
    int code;

    QoreStringNode* mr = priv->sendMsg(code, "STOR", remotename, xsink);
    if (!mr) {
        assert(*xsink);
        priv->data.close();
        return -1;
    }

    FtpResp resp(mr);
    if (*xsink) {
        priv->data.close();
        return -1;
    }

    if ((code / 100) != 1) {
        priv->data.close();
        xsink->raiseException("FTP-PUT-ERROR", "could not put file, FTP server replied: %s", resp.c_str());
        return -1;
    }

    if ((priv->mode == FTP_MODE_PORT && priv->acceptDataConnection(xsink)) || *xsink) {
        priv->data.close();
        return -1;
    }
    else if (priv->secure_data && priv->data.upgradeClientToSSL(xsink)) {
        return -1;
    }

    // issue #3032: use the correct timeout with the input stream
    priv->data.priv->sendFromInputStream(is, -1, priv->timeout_ms, xsink, &priv->m);
    priv->data.close();

    if (*xsink) {
        return -1;
    }

    resp.assign(priv->getResponse(code, xsink));
    sl.unlock();
    if (*xsink)
        return -1;

    //printf("PUT: %s", resp->c_str());
    if ((code / 100 != 2)) {
        xsink->raiseException("FTP-PUT-ERROR", "FTP server returned an error to the STOR command: %s", resp.c_str());
        return -1;
    }

    return 0;
}

// public locked
int QoreFtpClient::putData(const void *data, size_t len, const char* remotename, ExceptionSink* xsink) {
   assert(remotename);

   printd(5, "QoreFtpClient::putData(%p, %ld, %s)\n", data, len, remotename);

   SafeLocker sl(priv->m);
   if (priv->checkConnectedUnlocked(xsink))
      return -1;

   if (priv->use_async && priv->ctrl_op) {
      int rv = priv->putAsyncBlocking(data, len, remotename, xsink);
      sl.unlock();
      return rv;
   }

   // set binary mode and establish data connection
   if (priv->setBinaryMode(true, xsink) || priv->connectData(xsink)) {
      return -1;
   }

   // transfer file
   int code;

   QoreStringNode* mr = priv->sendMsg(code, "STOR", remotename, xsink);
   if (!mr) {
      assert(*xsink);
      priv->data.close();
      return -1;
   }

   FtpResp resp(mr);
   //printf("%s", resp->c_str());

   if ((code / 100) != 1) {
      priv->data.close();
      xsink->raiseException("FTP-PUT-ERROR", "could not put file, FTP server replied: %s", resp.c_str());
      return -1;
   }

   if ((priv->mode == FTP_MODE_PORT && priv->acceptDataConnection(xsink)) || *xsink) {
      priv->data.close();
      return -1;
   }
   else if (priv->secure_data && priv->data.upgradeClientToSSL(xsink, priv->timeout_ms))
      return -1;

   int rc = priv->data.send((const char*)data, len, priv->timeout_ms, xsink);
   priv->data.close();

   resp.assign(priv->getResponse(code, xsink));
   sl.unlock();
   if (*xsink)
      return -1;

   //printf("PUT: %s", resp->c_str());
   if ((code / 100 != 2)) {
      xsink->raiseException("FTP-PUT-ERROR", "FTP server returned an error to the STOR command: %s", resp.c_str());
      return -1;
   }

   if (rc) {
      xsink->raiseException("FTP-PUT-ERROR", "error sending file, may not be complete on target");
      return -1;
   }
   return 0;
}

// public locked
int QoreFtpClient::get(const char* remotepath, const char* localname, ExceptionSink* xsink) {
    printd(5, "QoreFtpClient::get(%s, %s)\n", remotepath, localname ? localname : "NULL");

    SafeLocker sl(priv->m);
    if (priv->checkConnectedUnlocked(xsink)) {
        return -1;
    }

    if (priv->use_async && priv->ctrl_op) {
        TmpLocalName ln(localname, remotepath);
        SimpleRefHolder<BinaryNode> data(priv->getAsyncBlocking(remotepath, xsink));
        sl.unlock();
        if (*xsink) {
            return -1;
        }
        // Write to file
        int fd = open(*ln, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            xsink->raiseErrnoException("FTP-FILE-OPEN-ERROR", errno, "%s", *ln);
            return -1;
        }
        if (*data && (*data)->size() > 0) {
            ssize_t nw = write(fd, (*data)->getPtr(), (*data)->size());
            if (nw < 0) {
                int en = errno;
                close(fd);
                xsink->raiseErrnoException("FTP-FILE-WRITE-ERROR", en, "error writing file");
                return -1;
            }
        }
        close(fd);
        return 0;
    }

    // get local file name
    TmpLocalName ln(localname, remotepath);
    printd(FTPDEBUG, "QoreFtpClient::get(%s) %s\n", remotepath, *ln);
    // open local file
    int fd = open(*ln, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (fd < 0) {
        xsink->raiseErrnoException("FTP-FILE-OPEN-ERROR", errno, "%s", *ln);
        return -1;
    }

    FtpResp resp;
    {
        ON_BLOCK_EXIT(close, fd);
        if (priv->pre_get(resp, remotepath, xsink)) {
            // delete temporary file
            unlink(*ln);
            return -1;
        }
        ln.discard();

        priv->data.recv(fd, -1, priv->timeout_ms, xsink);
        priv->data.close();
    }

    int code;
    resp.assign(priv->getResponse(code, xsink));
    sl.unlock();
    if (*xsink) {
        return -1;
    }

    //printf("PUT: %s", resp->c_str());
    if ((code / 100 != 2)) {
        xsink->raiseException("FTP-GET-ERROR", "FTP server returned an error to the RETR command: %s",
            resp.c_str());
        return -1;
    }
    return 0;
}

// public locked
int QoreFtpClient::get(const char* remotepath, OutputStream *os, ExceptionSink* xsink) {
   printd(5, "QoreFtpClient::get(%s, OutputStream)\n", remotepath);

   SafeLocker sl(priv->m);
   if (priv->checkConnectedUnlocked(xsink))
      return -1;

   if (priv->use_async && priv->ctrl_op) {
      // Download via async, write to OutputStream
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

   FtpResp resp;
   if (priv->pre_get(resp, remotepath, xsink)) {
      return -1;
   }

   priv->data.priv->recvToOutputStream(os, -1, priv->timeout_ms, xsink, &priv->m);
   priv->data.close();

   if (*xsink) {
      return -1;
   }

   int code;
   resp.assign(priv->getResponse(code, xsink));
   sl.unlock();
   if (*xsink)
      return -1;

   //printf("PUT: %s", resp->c_str());
   if ((code / 100 != 2)) {
      xsink->raiseException("FTP-GET-ERROR", "FTP server returned an error to the RETR command: %s",
                            resp.c_str());
      return -1;
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

    if (priv->use_async && priv->ctrl_op) {
        SimpleRefHolder<BinaryNode> data(priv->getAsyncBlocking(remotepath, xsink));
        sl.unlock();
        if (*xsink || !*data) {
            return nullptr;
        }
        return new QoreStringNode(static_cast<const char*>((*data)->getPtr()),
            (*data)->size(), encoding);
    }

    printd(FTPDEBUG, "QoreFtpClient::getAsString(%s)\n", remotepath);

    FtpResp resp;
    if (priv->pre_get(resp, remotepath, xsink)) {
        return nullptr;
    }

    SimpleRefHolder<QoreStringNode> rv(priv->data.recv(-1, priv->timeout_ms, xsink));
    priv->data.close();
    if (*xsink) {
        return nullptr;
    }

    int code;
    resp.assign(priv->getResponse(code, xsink));
    sl.unlock();

    if (*xsink) {
        return nullptr;
    }

    //printf("PUT: %s", resp->c_str());
    if ((code / 100 != 2)) {
        xsink->raiseException("FTP-GETASSTRING-ERROR", "FTP server returned an error to the RETR command: %s",
            resp.c_str());
        return nullptr;
    }
    // update string encoding
    if (encoding != QCS_DEFAULT) {
        qore_string_private* str = qore_string_private::get(*rv);
        str->encoding = encoding;
    }
    return rv.release();
}

// public locked
BinaryNode* QoreFtpClient::getAsBinary(const char* remotepath, ExceptionSink* xsink) {
    printd(5, "QoreFtpClient::getAsBinary(%s)\n", remotepath);

    SafeLocker sl(priv->m);
    if (priv->checkConnectedUnlocked(xsink)) {
        return nullptr;
    }

    if (priv->use_async && priv->ctrl_op) {
        BinaryNode* rv = priv->getAsyncBlocking(remotepath, xsink);
        sl.unlock();
        return rv;
    }

    printd(FTPDEBUG, "QoreFtpClient::getAsBinary(%s)\n", remotepath);

    FtpResp resp;
    if (priv->pre_get(resp, remotepath, xsink)) {
        return nullptr;
    }

    qore_offset_t rc;
    SimpleRefHolder<BinaryNode> rv(priv->data.priv->recvBinary(xsink, -1, priv->timeout_ms, rc, QORE_SOURCE_FTPCLIENT));
    priv->data.close();
    if (*xsink) {
        return nullptr;
    }

    int code;
    resp.assign(priv->getResponse(code, xsink));
    sl.unlock();

    if (*xsink) {
        return nullptr;
    }

    //printf("PUT: %s", resp->c_str());
    if ((code / 100 != 2)) {
        xsink->raiseException("FTP-GETASBINARY-ERROR", "FTP server returned an error to the RETR command: %s",
            resp.c_str());
        return nullptr;
    }
    return rv.release();
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
    return priv->control.getSSLCipherName();
}

const char* QoreFtpClient::getSSLCipherVersion() const {
    return priv->control.getSSLCipherVersion();
}

long QoreFtpClient::verifyPeerCertificate() const {
    return priv->control.verifyPeerCertificate();
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
    priv->setEventQueue(xsink, q, arg, xsink);
}

void QoreFtpClient::setControlEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
    priv->setControlEventQueue(xsink, q, arg, xsink);
}

void QoreFtpClient::setDataEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
    priv->setDataEventQueue(xsink, q, arg, xsink);
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

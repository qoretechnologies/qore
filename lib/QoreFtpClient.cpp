/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreFtpClient.cpp

    thread-safe QoreFtpClient object

    Qore Programming Language

    Copyright (C) 2003 - 2023 Qore Technologies, s.r.o.

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
#include "qore/intern/QC_Queue.h"
#include "qore/intern/qore_socket_private.h"
#include "qore/intern/qore_string_private.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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
        manual_mode = false;

    DLLLOCAL qore_ftp_private(const QoreString* url, ExceptionSink* xsink) {
        if (url) {
            setURLIntern(url, xsink);
        }
    }

    DLLLOCAL qore_ftp_private() {
    }

    DLLLOCAL ~qore_ftp_private() {
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
        return control.getPeerInfo(xsink, host_lookup);
    }

    DLLLOCAL QoreHashNode* getDataPeerInfo(ExceptionSink* xsink, bool host_lookup) const {
        AutoLocker al(m);
        return data.getPeerInfo(xsink, host_lookup);
    }

    DLLLOCAL QoreHashNode* getControlSocketInfo(ExceptionSink* xsink, bool host_lookup) const {
        AutoLocker al(m);
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
        return (!loggedin || !control.isOpen()) && connectUnlocked(xsink) ? -1 : 0;
    }

    DLLLOCAL void disconnectIntern() {
        control.close();
        control_connected = false;
        if (!manual_mode)
            mode = FTP_MODE_UNKNOWN;
        data.close();
        loggedin = false;
    }

    DLLLOCAL int connect(ExceptionSink* xsink) {
        SafeLocker sl(m);
        return connectUnlocked(xsink);
    }

    // unlocked
    DLLLOCAL QoreStringNode* getResponse(int &code, ExceptionSink* xsink) {
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
                    resp->concat(*r);
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

        if (control.upgradeClientToSSL(xsink, 0, 0, timeout_ms))
            return -1;

        if (secure_data)
            return doProt(resp, xsink);

        return 0;
    }

    DLLLOCAL int connectUnlocked(ExceptionSink* xsink) {
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

        if (secure_data && data.upgradeClientToSSL(xsink, 0, 0, timeout_ms))
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

        if (secure_data && data.upgradeClientToSSL(xsink, 0, 0, timeout_ms)) {
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

        if (getsockname(control.getSocket(), (struct sockaddr *)&add, &socksize) < 0) {
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
        else if (secure_data && data.upgradeClientToSSL(xsink, 0, 0, timeout_ms)) {
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
    } else if (priv->secure_data && priv->data.upgradeClientToSSL(xsink, 0, 0, priv->timeout_ms))
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
        l->concat(resp.getStr());
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
    } else if (priv->secure_data && priv->data.upgradeClientToSSL(xsink, 0, 0, priv->timeout_ms)) {
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
    else if (priv->secure_data && priv->data.upgradeClientToSSL(xsink, 0, 0)) {
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
   else if (priv->secure_data && priv->data.upgradeClientToSSL(xsink, 0, 0, priv->timeout_ms))
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
        qore_string_private* str = qore_string_private::get(**rv);
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

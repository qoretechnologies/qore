/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_SocketPollOperation.h

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

#ifndef _QORE_CLASS_SOCKETPOLLOPERATION_H

#define _QORE_CLASS_SOCKETPOLLOPERATION_H

#include "qore/intern/QC_SocketPollOperationBase.h"
#include "qore/intern/qore_socket_private.h"
#include "qore/QoreSocketObject.h"

#include <memory>

// goals: connect, connect-ssl
constexpr int SPG_CONNECT = 1;
constexpr int SPG_CONNECT_SSL = 2;

// states: none -> connecting -> [connecting-ssl ->] connected
constexpr int SPS_NONE = 0;
constexpr int SPS_CONNECTING = 1;
constexpr int SPS_CONNECTING_SSL = 2;
constexpr int SPS_CONNECTED = 3;

class SocketPollSocketOperationBase : public SocketPollOperationBase {
public:
    DLLLOCAL SocketPollSocketOperationBase(QoreObject* self) : SocketPollOperationBase(self) {
    }

    DLLLOCAL SocketPollSocketOperationBase(QoreSocketObject* sock) : sock(sock) {
    }

    DLLLOCAL ~SocketPollSocketOperationBase() {
    }

    DLLLOCAL virtual void abort(ExceptionSink* xsink) {
        if (set_non_block) {
            set_non_block = false;
            AutoLocker al(sock->priv->m);
            sock->priv->clearNonBlock();
            if (abortNeedsClose()) {
                sock->close();
            }
            state = SPS_NONE;
        }
    }

protected:
    std::unique_ptr<AbstractPollState> poll_state;
    QoreSocketObject* sock = nullptr;
    int state = SPS_NONE;
    bool set_non_block = false;

    DLLLOCAL virtual bool abortNeedsClose() const {
        return true;
    }
};

class SocketConnectPollOperation : public SocketPollSocketOperationBase {
public:
    DLLLOCAL SocketConnectPollOperation(ExceptionSink* xsink, bool ssl, const char* target, QoreSocketObject* sock);

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                sock->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const {
        return state == SPS_CONNECTED;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);

protected:
    //! Called in the constructor
    DLLLOCAL virtual int preVerify(ExceptionSink* xsink) {
        return 0;
    }

    //! Called when the connection is established
    DLLLOCAL virtual void connected();

    //! Called to switch to the connect-ssl state
    DLLLOCAL int startSslConnect(ExceptionSink* xsink);

private:
    std::string target;

    int sgoal = 0;

    DLLLOCAL virtual const char* getStateImpl() const {
        switch (state) {
            case SPS_NONE:
                return "none";
            case SPS_CONNECTING:
                return "connecting";
            case SPS_CONNECTING_SSL:
                return "connecting-ssl";
            case SPS_CONNECTED:
                return "connected";
            default:
                assert(false);
        }
        return "";
    }

    DLLLOCAL int checkContinuePoll(ExceptionSink* xsink);
};

class SocketSendPollOperation : public SocketPollSocketOperationBase {
public:
    // "data" must be passed already referenced
    DLLLOCAL SocketSendPollOperation(ExceptionSink* xsink, QoreStringNode* data, QoreSocketObject* sock);

    // "data" must be passed already referenced
    DLLLOCAL SocketSendPollOperation(ExceptionSink* xsink, BinaryNode* data, QoreSocketObject* sock);

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                sock->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const {
        return sent;
    }

    DLLLOCAL virtual const char* getStateImpl() const {
        return sent ? "sent" : "sending";
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);

private:
    SimpleRefHolder<SimpleValueQoreNode> data;
    const char* buf;
    size_t size;
    bool sent = false;

    DLLLOCAL virtual bool abortNeedsClose() const;
};

class SocketRecvPollOperationBase : public SocketPollSocketOperationBase {
public:
    DLLLOCAL SocketRecvPollOperationBase(QoreSocketObject* sock, bool to_string)
            : SocketPollSocketOperationBase(sock), to_string(to_string) {
    }

    DLLLOCAL virtual void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                sock->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const {
        return received;
    }

    DLLLOCAL virtual const char* getStateImpl() const {
        return received ? "received" : "receiving";
    }

    DLLLOCAL virtual QoreValue getOutput() const {
        return data ? data->refSelf() : QoreValue();
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);

protected:
    SimpleRefHolder<SimpleValueQoreNode> data;
    bool to_string;
    bool received = false;

    DLLLOCAL int initIntern(ExceptionSink* xsink);

    DLLLOCAL virtual bool abortNeedsClose() const = 0;
};

class SocketRecvPollOperation : public SocketRecvPollOperationBase {
public:
    // "data" must be passed already referenced
    DLLLOCAL SocketRecvPollOperation(ExceptionSink* xsink, ssize_t size, QoreSocketObject* sock, bool to_string);

private:
    size_t size;

    DLLLOCAL virtual bool abortNeedsClose() const;
};

class SocketRecvDataPollOperation : public SocketRecvPollOperationBase {
public:
    // "data" must be passed already referenced
    DLLLOCAL SocketRecvDataPollOperation(ExceptionSink* xsink, QoreSocketObject* sock, bool to_string);

    DLLLOCAL virtual bool abortNeedsClose() const;
};

class SocketRecvUntilBytesPollOperation : public SocketRecvPollOperationBase {
public:
    // "data" must be passed already referenced
    DLLLOCAL SocketRecvUntilBytesPollOperation(ExceptionSink* xsink, const QoreStringNode* pattern,
            QoreSocketObject* sock, bool to_string);

private:
    SimpleRefHolder<QoreStringNode> pattern;

    DLLLOCAL virtual bool abortNeedsClose() const;
};

class SocketReadHttpHeaderPollOperation : public SocketRecvPollOperationBase {
public:
    // "data" must be passed already referenced
    DLLLOCAL SocketReadHttpHeaderPollOperation(ExceptionSink* xsink, QoreSocketObject* sock);

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);

    DLLLOCAL virtual QoreValue getOutput() const;

private:
    mutable ReferenceHolder<QoreHashNode> out;

    DLLLOCAL virtual bool abortNeedsClose() const;
};

class SocketUpgradeClientSslPollOperation : public SocketPollSocketOperationBase {
public:
    DLLLOCAL SocketUpgradeClientSslPollOperation(ExceptionSink* xsink, QoreSocketObject* sock);

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                sock->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const {
        return done;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);

    DLLLOCAL virtual const char* getStateImpl() const {
        return "connecting-ssl";
    }

private:
    bool done = false;
};

class SocketUpgradeServerSslPollOperation : public SocketPollSocketOperationBase {
public:
    DLLLOCAL SocketUpgradeServerSslPollOperation(ExceptionSink* xsink, QoreSocketObject* sock);

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                sock->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const {
        return done;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);

    DLLLOCAL virtual const char* getStateImpl() const {
        return "accepting-ssl";
    }

private:
    bool done = false;
};

// goals: accept, accept-ssl
constexpr int SPG_ACCEPT = 1;
constexpr int SPG_ACCEPT_SSL = 2;

// states: none -> accepting -> [accepting-ssl ->] accepted
constexpr int SPS_ACCEPTING = 1;
constexpr int SPS_ACCEPTING_SSL = 2;
constexpr int SPS_ACCEPTED = 3;

class SocketAcceptPollSocketOperationBase : public SocketPollOperationBase {
public:
    DLLLOCAL SocketAcceptPollSocketOperationBase(QoreObject* self) : SocketPollOperationBase(self) {
    }

    DLLLOCAL SocketAcceptPollSocketOperationBase(QoreSocketObject* sock) : sock(sock) {
    }

    DLLLOCAL ~SocketAcceptPollSocketOperationBase() {
    }

    DLLLOCAL virtual void abort(ExceptionSink* xsink) {
        // NOTE: we do not close the socket here in any case
        if (set_non_block) {
            set_non_block = false;
            sock->clearNonBlock();
            state = SPS_NONE;
        }
    }

protected:
    QoreSocketObject* sock = nullptr;
    int state = SPS_NONE;
    bool set_non_block = false;

    DLLLOCAL virtual bool abortNeedsClose() const {
        return true;
    }
};

class SocketAcceptPollOperation : public SocketAcceptPollSocketOperationBase {
public:
    DLLLOCAL SocketAcceptPollOperation(ExceptionSink* xsink, QoreSocketObject* sock);

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                sock->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const {
        return state == SPS_ACCEPTED;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);

    DLLLOCAL virtual QoreValue getOutput() const;

protected:
    mutable SimpleRefHolder<QoreSocketObject> accepted_socket;

    //! Called in the constructor
    DLLLOCAL virtual int preVerify(ExceptionSink* xsink) {
        return 0;
    }

    //! Called when the connection is established
    DLLLOCAL virtual void accepted();

    //! Called to switch to the connect-ssl state
    DLLLOCAL int startSslAccept(ExceptionSink* xsink);

private:
    std::unique_ptr<AbstractPollState> poll_state;
    std::string target;

    int sgoal = 0;

    DLLLOCAL virtual const char* getStateImpl() const {
        switch (state) {
            case SPS_NONE:
                return "none";
            case SPS_ACCEPTING:
                return "accepting";
            case SPS_ACCEPTING_SSL:
                return "accepting-ssl";
            case SPS_ACCEPTED:
                return "accepted";
            default:
                assert(false);
        }
        return "";
    }

    DLLLOCAL int checkContinuePoll(ExceptionSink* xsink);
};

#ifdef HAVE_HTTP2

class Http2Session;

// HTTP/2 poll operation states
constexpr int H2S_NONE = 0;
constexpr int H2S_SEND_PREFACE = 1;
constexpr int H2S_RECV_PREFACE = 2;
constexpr int H2S_READING = 3;
constexpr int H2S_REQUEST_READY = 4;
constexpr int H2S_SENDING = 5;
constexpr int H2S_SENT = 6;

//! Poll operation for reading HTTP/2 requests on a server connection
/** This poll operation handles HTTP/2 server-side request reading:
    1. Exchange connection preface (SETTINGS frames)
    2. Read frames until a complete request stream is available

    The output is a hash with request information:
    - method: HTTP method
    - path: request path
    - headers: request headers hash
    - body: request body (binary)
    - stream_id: HTTP/2 stream ID for sending response
    - session: the Http2Session for sending responses
*/
class SocketHttp2ServerPollOperation : public SocketPollSocketOperationBase {
public:
    DLLLOCAL SocketHttp2ServerPollOperation(ExceptionSink* xsink, QoreSocketObject* sock);

    DLLLOCAL ~SocketHttp2ServerPollOperation();

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                sock->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const {
        return h2_state == H2S_REQUEST_READY;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);

    DLLLOCAL virtual QoreValue getOutput() const;

    DLLLOCAL virtual const char* getStateImpl() const {
        switch (h2_state) {
            case H2S_NONE: return "none";
            case H2S_SEND_PREFACE: return "sending-preface";
            case H2S_RECV_PREFACE: return "receiving-preface";
            case H2S_READING: return "reading-frames";
            case H2S_REQUEST_READY: return "request-ready";
            default: return "unknown";
        }
    }

    //! Returns the Http2Session (caller takes ownership)
    DLLLOCAL Http2Session* takeSession();

    //! Returns the stream ID of the completed request
    DLLLOCAL int32_t getStreamId() const { return stream_id; }

private:
    std::unique_ptr<Http2Session> h2_session;
    int h2_state = H2S_NONE;
    int32_t stream_id = 0;
    mutable ReferenceHolder<QoreHashNode> out;

    DLLLOCAL virtual bool abortNeedsClose() const { return true; }

    //! Initialize HTTP/2 session
    DLLLOCAL int initSession(ExceptionSink* xsink);
};

//! Poll operation for sending HTTP/2 responses
/** This poll operation sends an HTTP/2 response:
    - Submits response headers and body
    - Sends all pending frames

    The HTTP/2 session is retrieved from the socket (stored by a previous
    read operation).
*/
class SocketHttp2SendResponsePollOperation : public SocketPollSocketOperationBase {
public:
    //! Creates the poll operation
    /** @param xsink exception sink
        @param sock the socket (must have an active HTTP/2 session from a previous read)
        @param h2_session unused, session is retrieved from socket
        @param stream_id the stream ID to respond on
        @param status_code HTTP status code
        @param headers response headers
        @param body response body (can be nullptr)
    */
    DLLLOCAL SocketHttp2SendResponsePollOperation(ExceptionSink* xsink, QoreSocketObject* sock,
        Http2Session* h2_session, int32_t stream_id, int status_code,
        const QoreHashNode* headers, const BinaryNode* body);

    DLLLOCAL ~SocketHttp2SendResponsePollOperation();

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                sock->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const {
        return h2_state == H2S_SENT;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);

    DLLLOCAL virtual const char* getStateImpl() const {
        switch (h2_state) {
            case H2S_NONE: return "none";
            case H2S_SENDING: return "sending";
            case H2S_SENT: return "sent";
            default: return "unknown";
        }
    }

    //! Returns nullptr - session is managed by socket
    DLLLOCAL Http2Session* takeSession();

private:
    int h2_state = H2S_NONE;
    int32_t stream_id = 0;

    DLLLOCAL virtual bool abortNeedsClose() const { return true; }
};

#endif // HAVE_HTTP2

DLLLOCAL QoreClass* initSocketPollOperationClass(QoreNamespace& qorens);

#endif // _QORE_CLASS_SOCKETPOLLOPERATION_H

/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreSocketObject.h

    Qore Programming Language

    Copyright (C) 2003 - 2025 Qore Technologies, s.r.o.

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

#ifndef _QORE_QORE_SOCKET_OBJECT_H

#define _QORE_QORE_SOCKET_OBJECT_H

#include <qore/QoreSocket.h>
#include <qore/AbstractPollableIoObjectBase.h>
#include <qore/QoreThreadLock.h>

class QoreSSLCertificate;
class QoreSSLPrivateKey;
class Queue;
class my_socket_priv;

class QoreSocketObject : public AbstractPollableIoObjectBase {
    friend class my_socket_priv;
    friend struct qore_httpclient_priv;
    friend class SocketPollSocketOperationBase;
    friend class SocketConnectPollOperation;
    friend class SocketAcceptPollSocketOperationBase;
    friend class SocketAcceptPollOperation;
    friend class SocketSendPollOperation;
    friend class SocketRecvPollOperationBase;
    friend class SocketRecvPollOperation;
    friend class SocketRecvDataPollOperation;
    friend class SocketReadHttpHeaderPollOperation;
    friend class SocketRecvUntilBytesPollOperation;
    friend class SocketUpgradeClientSslPollOperation;
    friend class SocketUpgradeServerSslPollOperation;
    friend class HttpClientConnectPollOperation;
    friend class SocketHttp2ServerPollOperation;
    friend class SocketHttp2SendResponsePollOperation;

public:
    DLLEXPORT QoreSocketObject();

    DLLEXPORT QoreSocketObject(QoreSocketObject& orig, int descriptor);

    DLLEXPORT virtual void deref(ExceptionSink* xsink);
    DLLEXPORT virtual void deref();

    //! Invalidates the socket
    /** Normally called only in the %Qore object's destructor

        @since %Qore 1.12
    */
    DLLEXPORT void invalidate(ExceptionSink* xsink);

    //! Starts a non-blocking socket connection to the given target
    /** @param xsink %Qore-language exceptions will be raised here
        @param name the connection target (ex: "host:port" or "/tmp/socket-file", etc)

        @return a socket poll state object or nullptr in case of an exception or an immediate connection

        @since %Qore 1.12
    */
    DLLEXPORT AbstractPollState* startConnect(ExceptionSink* xsink, const char* name);

    //! Starts a non-blocking client SSL upgrade on a connected socket
    /** @param xsink %Qore-language exceptions will be raised here

        @return a socket poll state object or nullptr in case of an exception or an immediate SSL connection

        @since %Qore 1.12
    */
    DLLEXPORT AbstractPollState* startSslConnect(ExceptionSink* xsink);

    //! Starts a non-blocking send operation on a connected socket
    /**
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param data the data to send, must stay valid for the lifetime of the AbstractPollState object returned
        @param size the size of the data to send

        @return a socket poll state object or nullptr in case of an exception or an immediate send

        @since %Qore 1.12
    */
    DLLEXPORT AbstractPollState* startSend(ExceptionSink* xsink, const char* data, size_t size);

    //! Starts a non-blocking accept operation to a socket and returns a poll state object
    /** The socket must be already bound and listening

        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return a socket poll state object or nullptr in case of an exception or an immediate accept

        @since %Qore 2.0
    */
    DLLEXPORT AbstractPollState* startAccept(ExceptionSink* xsink);

    //! Starts a non-blocking server SSL upgrade on a connected socket
    /** @param xsink %Qore-language exceptions will be raised here

        @return a socket poll state object or nullptr in case of an exception or an immediate SSL connection

        @since %Qore 2.0
    */
    DLLEXPORT AbstractPollState* startSslAccept(ExceptionSink* xsink);

    //! Starts a non-blocking receive operation on a connected socket
    /**
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param size the size of the data to read, must be > 0

        @return a socket poll state object or nullptr in case of an exception or an immediate receive

        @since %Qore 1.12
    */
    DLLEXPORT AbstractPollState* startRecv(ExceptionSink* xsink, size_t size);

    //! Starts a non-blocking receive operation on a connected socket
    /**
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param pattern the bytes to expect that indicate the end of the data, must stay valid for the lifetime of the
        AbstractPollState object returned
        @param size the size of pattern in bytes

        @return a socket poll state object or nullptr in case of an exception or an immediate receive

        @since %Qore 1.12
    */
    DLLEXPORT AbstractPollState* startRecvUntilBytes(ExceptionSink* xsink, const char* pattern, size_t size);

    //! Starts a non-blocking receive packet operation on a connected socket
    /**
        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return a socket poll state object or nullptr in case of an exception or an immediate receive

        @since %Qore 2.0
    */
    DLLEXPORT AbstractPollState* startRecvPacket(ExceptionSink* xsink);

    DLLEXPORT int connect(const char* name, int timeout_ms, ExceptionSink* xsink = nullptr);
    DLLEXPORT int connectINET(const char* host, int port, int timeout_ms, ExceptionSink* xsink = nullptr);
    DLLEXPORT int connectINET2(const char* host, const char* service, int family, int sock_type, int protocol,
            int timeout_ms = -1, ExceptionSink* xsink = nullptr);
    DLLEXPORT int connectUNIX(const char* p, int socktype, int protocol, ExceptionSink* xsink = nullptr);
    DLLEXPORT int connectSSL(ExceptionSink* xsink, const char* name, int timeout_ms);
    DLLEXPORT int connectINETSSL(ExceptionSink* xsink, const char* host, int port, int timeout_ms);
    DLLEXPORT int connectINET2SSL(ExceptionSink* xsink, const char* host, const char* service, int family,
            int sock_type, int protocol, int timeout_ms = -1);
    DLLEXPORT int connectUNIXSSL(ExceptionSink* xsink, const char* p, int socktype, int protocol);
    // to bind to either a UNIX socket or an INET interface:port
    DLLEXPORT int bind(const char* name, bool reuseaddr = false);
    // to bind to an INET tcp port on all interfaces
    DLLEXPORT int bind(int port, bool reuseaddr = false);
    // to bind an open socket to an INET tcp port on a specific interface
    DLLEXPORT int bind(const char* iface, int port, bool reuseaddr = false);

    DLLEXPORT int bindUNIX(const char* name, int socktype, int protocol, ExceptionSink* xsink);
    DLLEXPORT int bindINET(const char* name, const char* service, bool reuseaddr, int family, int socktype,
            int protocol, ExceptionSink* xsink);

    // get port number for INET sockets
    DLLEXPORT int getPort();
    DLLEXPORT QoreSocketObject* accept(SocketSource* source, ExceptionSink* xsink);
    DLLEXPORT QoreSocketObject* acceptSSL(ExceptionSink* xsink, SocketSource* source);
    DLLEXPORT QoreSocketObject* accept(int timeout_ms, ExceptionSink* xsink);
    DLLEXPORT QoreSocketObject* acceptSSL(ExceptionSink* xsink, int timeout_ms);

    DLLEXPORT int listen(int backlog);
    // send a buffer of a particular size
    DLLEXPORT int send(const char* buf, int size);
    DLLEXPORT int send(const char* buf, int size, int timeout_ms, ExceptionSink* xsink);
    // send a null-terminated string
    DLLEXPORT int send(const QoreStringNode& msg, int timeout_ms, ExceptionSink* xsink);
    // send a binary object
    DLLEXPORT int send(const BinaryNode* b);
    DLLEXPORT int send(const BinaryNode* b, int timeout_ms, ExceptionSink* xsink);
    // send a certain number of bytes (read from an InputStream)
    DLLEXPORT void sendFromInputStream(InputStream* is, int64 size, int64 timeout_ms, ExceptionSink *xsink);

    // send from a file descriptor
    DLLEXPORT int send(int fd, int size = -1);
    // send bytes and convert to network order
    DLLEXPORT int sendi1(char b, int timeout_ms, ExceptionSink* xsink);
    DLLEXPORT int sendi2(short b, int timeout_ms, ExceptionSink* xsink);
    DLLEXPORT int sendi4(int b, int timeout_ms, ExceptionSink* xsink);
    DLLEXPORT int sendi8(int64 b, int timeout_ms, ExceptionSink* xsink);
    DLLEXPORT int sendi2LSB(short b, int timeout_ms, ExceptionSink* xsink);
    DLLEXPORT int sendi4LSB(int b, int timeout_ms, ExceptionSink* xsink);
    DLLEXPORT int sendi8LSB(int64 b, int timeout_ms, ExceptionSink* xsink);
    // receive a message
    DLLEXPORT QoreStringNode* recv(int timeout, ExceptionSink* xsink);
    // receive a certain number of bytes as a string
    DLLEXPORT QoreStringNode* recv(qore_offset_t bufsize, int timeout_ms, ExceptionSink* xsink);
    // receive a certain number of bytes as a binary object
    DLLEXPORT BinaryNode* recvBinary(int bufsize, int timeout, ExceptionSink* xsink);
    // receive a packet of bytes as a binary object
    DLLEXPORT BinaryNode* recvBinary(int timeout, ExceptionSink* xsink);
    // receive a certain number of bytes and write them to an OutputStream
    DLLEXPORT void recvToOutputStream(OutputStream* os, int64 size, int64 timeout_ms, ExceptionSink *xsink);

    // receive and write data to a file descriptor
    DLLEXPORT int recv(int fd, int size, int timeout);
    // receive integers and convert from network byte order
    DLLEXPORT int64 recvi1(int timeout, char* b, ExceptionSink* xsink);
    DLLEXPORT int64 recvi2(int timeout, short* b, ExceptionSink* xsink);
    DLLEXPORT int64 recvi4(int timeout, int* b, ExceptionSink* xsink);
    DLLEXPORT int64 recvi8(int timeout, int64* b, ExceptionSink* xsink);
    DLLEXPORT int64 recvi2LSB(int timeout, short* b, ExceptionSink* xsink);
    DLLEXPORT int64 recvi4LSB(int timeout, int* b, ExceptionSink* xsink);
    DLLEXPORT int64 recvi8LSB(int timeout, int64* b, ExceptionSink* xsink);
    DLLEXPORT int64 recvu1(int timeout, unsigned char* b, ExceptionSink* xsink);
    DLLEXPORT int64 recvu2(int timeout, unsigned short* b, ExceptionSink* xsink);
    DLLEXPORT int64 recvu4(int timeout, unsigned int* b, ExceptionSink* xsink);
    DLLEXPORT int64 recvu2LSB(int timeout, unsigned short* b, ExceptionSink* xsink);
    DLLEXPORT int64 recvu4LSB(int timeout, unsigned int* b, ExceptionSink* xsink);

    // send HTTP message
    DLLEXPORT int sendHTTPMessage(ExceptionSink* xsink, QoreHashNode* info, const char* method, const char* path,
        const char* http_version, const QoreHashNode* headers, const void* ptr, int size, int source, int timeout_ms);
    DLLEXPORT int sendHTTPMessage(ExceptionSink* xsink, QoreHashNode* info, const char* method, const char* path,
        const char* http_version, const QoreHashNode* headers, const QoreStringNode& body, int source, int timeout_ms);
    DLLEXPORT int sendHTTPMessageWithCallback(ExceptionSink* xsink, QoreHashNode* info, const char* method,
        const char *path, const char *http_version, const QoreHashNode* headers,
        const ResolvedCallReferenceNode& send_callback, int source, int timeout_ms, bool* aborted = nullptr);

    // send HTTP response
    DLLEXPORT int sendHTTPResponse(ExceptionSink* xsink, QoreHashNode* info, int code, const char* desc,
        const char* http_version, const QoreHashNode* headers, const void* data, size_t size, int source,
        int timeout_ms);
    DLLEXPORT int sendHTTPResponse(ExceptionSink* xsink, QoreHashNode* info, int code, const char* desc,
        const char* http_version, const QoreHashNode* headers, const QoreStringNode& body, int source,
        int timeout_ms);

    // send HTTP response from stream
    DLLEXPORT int sendHTTPResponse(ExceptionSink* xsink, QoreHashNode* info, int code, const char* desc,
        const char* http_version, const QoreHashNode* headers, InputStream* is, size_t max_chunked_size,
        const ResolvedCallReferenceNode* trailer_callback, int source, int timeout_ms);

    // send HTTP response from callback
    DLLEXPORT int sendHTTPResponseWithCallback(ExceptionSink* xsink, QoreHashNode* info, int code, const char *desc,
        const char *http_version, const QoreHashNode* headers, const ResolvedCallReferenceNode& send_callback,
        int source, int timeout_ms, bool* aborted = nullptr);

    // send data in HTTP chunked format
    DLLEXPORT void sendHTTPChunkedBodyFromInputStream(InputStream* is, size_t max_chunked_size, const int timeout_ms,
        const ResolvedCallReferenceNode* trailer_callback, ExceptionSink* xsink);
    DLLEXPORT void sendHTTPChunkedBodyTrailer(const QoreHashNode* headers, int timeout_ms, ExceptionSink* xsink);

    // read and parse HTTP header
    DLLEXPORT AbstractQoreNode* readHTTPHeader(ExceptionSink* xsink, QoreHashNode* info, int timeout);
    // receive a binary message in HTTP chunked format
    DLLEXPORT QoreHashNode* readHTTPChunkedBodyBinary(int timeout, ExceptionSink* xsink);
    // receive a binary message in HTTP chunked format
    DLLEXPORT QoreHashNode* readHTTPChunkedBodyToOutputStream(OutputStream* os, int timeout_ms, ExceptionSink* xsink);
    // receive a string message in HTTP chunked format
    DLLEXPORT QoreHashNode* readHTTPChunkedBody(int timeout, ExceptionSink* xsink);

    // receive a single HTTP chunk
    DLLEXPORT QoreHashNode* readHttpChunk(int timeout, ExceptionSink* xsink);

    // receive a binary message in HTTP chunked format
    DLLEXPORT void readHTTPChunkedBodyBinaryWithCallback(const ResolvedCallReferenceNode& recv_callback,
            QoreObject* obj, int timeout_ms, ExceptionSink* xsink);
    // receive a string message in HTTP chunked format
    DLLEXPORT void readHTTPChunkedBodyWithCallback(const ResolvedCallReferenceNode& recv_callback, QoreObject* obj,
            int timeout_ms, ExceptionSink* xsink);

    DLLEXPORT QoreStringNode* readHTTPHeaderString(ExceptionSink* xsink, int timeout_ms);

    DLLEXPORT QoreHashNode* readServerSentEvent(ExceptionSink* xsink, const QoreStringNode* content_encoding,
            int timeout_ms);

    //! Returns the underlying file descriptor; -1 if not open
    /** @return the underlying file descriptor; -1 if not open

        @since %Qore 1.12
    */
    DLLEXPORT int getPollableDescriptor() const;

    DLLEXPORT int setSendTimeout(int ms);
    DLLEXPORT int setRecvTimeout(int ms);
    DLLEXPORT int getSendTimeout();
    DLLEXPORT int getRecvTimeout();
    DLLEXPORT int close();
    DLLEXPORT int shutdown();
    DLLEXPORT int shutdownSSL(ExceptionSink* xsink) ;
    DLLEXPORT const char* getSSLCipherName();
    DLLEXPORT const char* getSSLCipherVersion();
    DLLEXPORT bool isSecure();

    //! Sets the ALPN protocols to offer during TLS negotiation
    /** @param protocols A list of protocol names in order of preference (e.g., {"h2", "http/1.1"})
        @param xsink Exception sink for error handling

        @note This method must be called before connecting or upgrading to SSL/TLS

        @since %Qore 2.2
    */
    DLLEXPORT void setAlpnProtocols(const QoreListNode* protocols, ExceptionSink* xsink);

    //! Returns the negotiated ALPN protocol after a TLS connection is established
    /** @return The negotiated protocol string, or nullptr if no protocol was negotiated

        @since %Qore 2.2
    */
    DLLEXPORT QoreStringNode* getAlpnProtocol() const;

    //! Returns true if the connection is using HTTP/2 (negotiated via ALPN)
    /** @return true if HTTP/2, false otherwise

        @since %Qore 2.2
    */
    DLLEXPORT bool isHttp2() const;

    //! Submits an HTTP/2 PUSH_PROMISE frame for server push
    /** @since %Qore 2.2
    */
    DLLEXPORT int32_t submitHttp2PushPromise(int32_t stream_id, const char* path,
        const QoreHashNode* headers, ExceptionSink* xsink);

    //! Sets the active HTTP/2 stream ID for transparent send/recv operations
    /** @since %Qore 2.3
    */
    DLLEXPORT void setHttp2ActiveStream(int32_t stream_id, ExceptionSink* xsink);

    //! Gets the active HTTP/2 stream ID
    /** @return the active stream ID, or -1 if no stream is active
        @since %Qore 2.3
    */
    DLLEXPORT int32_t getHttp2ActiveStream() const;

    DLLEXPORT long verifyPeerCertificate();
    DLLEXPORT int getSocket();
    DLLEXPORT void setEncoding(const QoreEncoding* id);
    DLLEXPORT const QoreEncoding* getEncoding() const;
    DLLEXPORT bool isDataAvailable(ExceptionSink* xsink, int timeout = 0);
    DLLEXPORT bool isWriteFinished(ExceptionSink* xsink, int timeout = 0);
    DLLEXPORT bool isOpen() const;
    // c must be already referenced before this call
    DLLEXPORT void setCertificate(QoreSSLCertificate* c);
    // p must be already referenced before this call
    DLLEXPORT void setPrivateKey(QoreSSLPrivateKey* p);
    // c and p must be already referenced before this call
    DLLEXPORT void setCertificateAndPrivateKey(QoreSSLCertificate* c, QoreSSLPrivateKey* p);
    DLLEXPORT int setNoDelay(int nodelay);
    DLLEXPORT int getNoDelay();
    DLLEXPORT void setEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data);
    DLLEXPORT void setEventQueue(Queue* cbq, ExceptionSink* xsink);
    DLLEXPORT QoreHashNode* getPeerInfo(ExceptionSink* xsink, bool host_lookup = true) const;
    DLLEXPORT QoreHashNode* getSocketInfo(ExceptionSink* xsink, bool host_lookup = true) const;

    DLLEXPORT void upgradeClientToSSL(ExceptionSink* xsink, int timeout_ms = -1);
    DLLEXPORT void upgradeServerToSSL(ExceptionSink* xsink, int timeout_ms = -1);

    DLLEXPORT void clearWarningQueue(ExceptionSink* xsink);
    DLLEXPORT void setWarningQueue(ExceptionSink* xsink, int64 warning_ms, int64 warning_bs, Queue* wq, QoreValue arg,
            int64 min_ms = 1000);
    DLLEXPORT QoreHashNode* getUsageInfo() const;
    DLLEXPORT void clearStats();
    DLLEXPORT bool pendingHttpChunkedBody() const;
    DLLEXPORT void setSslVerifyMode(int mode);
    DLLEXPORT int getSslVerifyMode() const;
    DLLEXPORT void acceptAllCertificates(bool accept_all = true);
    DLLEXPORT bool getAcceptAllCertificates() const;
    DLLEXPORT bool captureRemoteCertificates(bool set);
    DLLEXPORT QoreObject* getRemoteCertificate() const;
    DLLEXPORT int64 getConnectionId() const;

    //! Sets the non-blocking connection flag
    DLLEXPORT int setNonBlock(ExceptionSink* xsink);

    //! Clears the non-blocking connection flag
    DLLEXPORT void clearNonBlock();

private:
    DLLLOCAL QoreSocketObject(QoreSocket* s, QoreSSLCertificate* cert = nullptr, QoreSSLPrivateKey* pk = nullptr);

protected:
    my_socket_priv* priv;

    DLLLOCAL virtual ~QoreSocketObject();
};

#endif // _QORE_QORE_SOCKET_OBJECT_H

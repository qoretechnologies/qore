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
#include "qore/intern/qore_socket_private.h"
#include "qore/intern/QC_Socket.h"
#include "qore/intern/QC_SSLCertificate.h"
#include "qore/intern/QC_SSLPrivateKey.h"
#include "qore/intern/Http2Session.h"

QoreSocketObject::QoreSocketObject(QoreSocket* s, QoreSSLCertificate* cert, QoreSSLPrivateKey* pk)
        : priv(new my_socket_priv(s, cert, pk)) {
}

QoreSocketObject::QoreSocketObject(QoreSocketObject& orig, int descriptor)
        : priv(new my_socket_priv(new QoreSocket(descriptor, orig.priv->socket->priv->sfamily,
            orig.priv->socket->priv->stype,
            orig.priv->socket->priv->sprot, orig.priv->socket->priv->enc),
            orig.priv->cert ? orig.priv->cert->certRefSelf() : nullptr,
            orig.priv->pk ? orig.priv->pk->pkRefSelf() : nullptr)) {
    // Copy ALPN protocols from the listener socket to the accepted socket so that
    // SSL handshakes on the accepted socket correctly negotiate HTTP/2 via ALPN
    if (!orig.priv->socket->priv->alpn_protocols.empty()) {
        priv->socket->priv->alpn_protocols = orig.priv->socket->priv->alpn_protocols;
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

void QoreSocketObject::invalidate(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    priv->invalidate();
    priv->socket->cleanup(xsink);
}

AbstractPollState* QoreSocketObject::startConnect(ExceptionSink* xsink, const char* name) {
    AutoLocker al(priv->m);
    return priv->socket->startConnect(xsink, name);
}

AbstractPollState* QoreSocketObject::startSslConnect(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    return priv->socket->startSslConnect(xsink, priv->cert, priv->pk);
}

AbstractPollState* QoreSocketObject::startSend(ExceptionSink* xsink, const char* data, size_t size) {
    AutoLocker al(priv->m);
    return priv->socket->startSend(xsink, data, size);
}

AbstractPollState* QoreSocketObject::startSslAccept(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    return priv->socket->startSslAccept(xsink, priv->cert, priv->pk);
}

AbstractPollState* QoreSocketObject::startRecv(ExceptionSink* xsink, size_t size) {
    AutoLocker al(priv->m);
    return priv->socket->startRecv(xsink, size);
}

AbstractPollState* QoreSocketObject::startRecvUntilBytes(ExceptionSink* xsink, const char* pattern, size_t size) {
    AutoLocker al(priv->m);
    return priv->socket->startRecvUntilBytes(xsink, pattern, size);
}

AbstractPollState* QoreSocketObject::startRecvPacket(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    return priv->socket->startRecvPacket(xsink);
}

/*
int QoreSocketObject::startAccept(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    return priv->socket->startAccept(xsink);
}

int QoreSocketObject::startSslAccept(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    return priv->socket->startSslAccept(xsink, priv->cert, priv->pk);
}
*/

int QoreSocketObject::connect(const char* name, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->connect(name, timeout_ms, xsink);
}

int QoreSocketObject::connectINET(const char* host, int port, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->connectINET(host, port, timeout_ms, xsink);
}

int QoreSocketObject::connectINET2(const char* name, const char* service, int family, int sock_type, int protocol,
        int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->connectINET2(name, service, family, sock_type, protocol, timeout_ms, xsink);
}

int QoreSocketObject::connectUNIX(const char* p, int sock_type, int protocol, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->connectUNIX(p, sock_type, protocol, xsink);
}

// to bind to either a UNIX socket or an INET interface:port
int QoreSocketObject::bind(const char* name, bool reuseaddr) {
    AutoLocker al(priv->m);
    return priv->socket->bind(name, reuseaddr);
}

// to bind to an INET tcp port on all interfaces
int QoreSocketObject::bind(int port, bool reuseaddr) {
    AutoLocker al(priv->m);
    return priv->socket->bind(port, reuseaddr);
}

// to bind an open socket to an INET tcp port on a specific interface
int QoreSocketObject::bind(const char* iface, int port, bool reuseaddr) {
    AutoLocker al(priv->m);
    return priv->socket->bind(iface, port, reuseaddr);
}

int QoreSocketObject::bindUNIX(const char* name, int socktype, int protocol, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->bindUNIX(name, socktype, protocol, xsink);
}

int QoreSocketObject::bindINET(const char* name, const char* service, bool reuseaddr, int family, int socktype,
        int protocol, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->bindINET(name, service, reuseaddr, family, socktype, protocol, xsink);
}

// get port number for INET sockets
int QoreSocketObject::getPort() {
    AutoLocker al(priv->m);
    return priv->socket->getPort();
}

int QoreSocketObject::listen(int backlog) {
    AutoLocker al(priv->m);
    return priv->socket->listen(backlog);
}

// send a buffer of a particular size
int QoreSocketObject::send(const char* buf, int size) {
    AutoLocker al(priv->m);
    return priv->socket->send(buf, size);
}

int QoreSocketObject::send(const char* buf, int size, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_SEND)) {
        return -1;
    }
    return priv->socket->send(buf, size, timeout_ms, xsink);
}

// send a null-terminated string
int QoreSocketObject::send(const QoreStringNode& msg, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_SEND)) {
        return -1;
    }
    return priv->socket->send(msg, timeout_ms, xsink);
}

// send a binary object
int QoreSocketObject::send(const BinaryNode* b, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_SEND)) {
        return -1;
    }
    return priv->socket->send(b, timeout_ms, xsink);
}

int QoreSocketObject::send(const BinaryNode* b) {
    AutoLocker al(priv->m);
    return priv->socket->send(b);
}

void QoreSocketObject::sendFromInputStream(InputStream *is, int64 size, int64 timeout_ms, ExceptionSink *xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_SEND)) {
        return;
    }
    priv->socket->priv->sendFromInputStream(is, size, timeout_ms, xsink, &priv->m);
}

// send from a file descriptor
int QoreSocketObject::send(int fd, int size) {
    AutoLocker al(priv->m);
    return priv->socket->send(fd, size);
}

// send bytes and convert to network order
int QoreSocketObject::sendi1(char b, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_SEND)) {
        return -1;
    }
    return priv->socket->sendi1(b, timeout_ms, xsink);
}

int QoreSocketObject::sendi2(short b, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_SEND)) {
        return -1;
    }
    return priv->socket->sendi2(b, timeout_ms, xsink);
}

int QoreSocketObject::sendi4(int b, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_SEND)) {
        return -1;
    }
    return priv->socket->sendi4(b, timeout_ms, xsink);
}

int QoreSocketObject::sendi8(int64 b, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_SEND)) {
        return -1;
    }
    return priv->socket->sendi8(b, timeout_ms, xsink);
}

int QoreSocketObject::sendi2LSB(short b, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_SEND)) {
        return -1;
    }
    return priv->socket->sendi2LSB(b, timeout_ms, xsink);
}

int QoreSocketObject::sendi4LSB(int b, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_SEND)) {
        return -1;
    }
    return priv->socket->sendi4LSB(b, timeout_ms, xsink);
}

int QoreSocketObject::sendi8LSB(int64 b, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_SEND)) {
        return -1;
    }
    return priv->socket->sendi8LSB(b, timeout_ms, xsink);
}

// receive a packet of bytes as a string
QoreStringNode* QoreSocketObject::recv(int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return nullptr;
    }
    return priv->socket->recv(timeout_ms, xsink);
}

// receive a certain number of bytes as a string
QoreStringNode* QoreSocketObject::recv(qore_offset_t bufsize, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return nullptr;
    }
    return priv->socket->recv(bufsize, timeout_ms, xsink);
}

// receive a packet of bytes as a binary
BinaryNode* QoreSocketObject::recvBinary(int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return nullptr;
    }
    return priv->socket->recvBinary(timeout_ms, xsink);
}

// receive a certain number of bytes as a binary object
BinaryNode* QoreSocketObject::recvBinary(int bufsize, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return nullptr;
    }
    return priv->socket->recvBinary(bufsize, timeout_ms, xsink);
}

void QoreSocketObject::recvToOutputStream(OutputStream *os, int64 size, int64 timeout_ms, ExceptionSink *xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return;
    }
    priv->socket->priv->recvToOutputStream(os, size, timeout_ms, xsink, &priv->m);
}

// receive and write data to a file descriptor
int QoreSocketObject::recv(int fd, int size, int timeout_ms) {
    AutoLocker al(priv->m);
    return priv->socket->recv(fd, size, timeout_ms);
}

// receive integers and convert from network byte order
int64 QoreSocketObject::recvi1(int timeout_ms, char* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return -1;
    }
    return priv->socket->recvi1(timeout_ms, b, xsink);
}

int64 QoreSocketObject::recvi2(int timeout_ms, short* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return -1;
    }
    return priv->socket->recvi2(timeout_ms, b, xsink);
}

int64 QoreSocketObject::recvi4(int timeout_ms, int* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return -1;
    }
    return priv->socket->recvi4(timeout_ms, b, xsink);
}

int64 QoreSocketObject::recvi8(int timeout_ms, int64* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return -1;
    }
    return priv->socket->recvi8(timeout_ms, b, xsink);
}

int64 QoreSocketObject::recvi2LSB(int timeout_ms, short* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return -1;
    }
    return priv->socket->recvi2LSB(timeout_ms, b, xsink);
}

int64 QoreSocketObject::recvi4LSB(int timeout_ms, int* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return -1;
    }
    return priv->socket->recvi4LSB(timeout_ms, b, xsink);
}

int64 QoreSocketObject::recvi8LSB(int timeout_ms, int64* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return -1;
    }
    return priv->socket->recvi8LSB(timeout_ms, b, xsink);
}

// receive integers and convert from network byte order
int64 QoreSocketObject::recvu1(int timeout_ms, unsigned char* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return -1;
    }
    return priv->socket->recvu1(timeout_ms, b, xsink);
}

int64 QoreSocketObject::recvu2(int timeout_ms, unsigned short* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return -1;
    }
    return priv->socket->recvu2(timeout_ms, b, xsink);
}

int64 QoreSocketObject::recvu4(int timeout_ms, unsigned int* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return -1;
    }
    return priv->socket->recvu4(timeout_ms, b, xsink);
}

int64 QoreSocketObject::recvu2LSB(int timeout_ms, unsigned short* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return -1;
    }
    return priv->socket->recvu2LSB(timeout_ms, b, xsink);
}

int64 QoreSocketObject::recvu4LSB(int timeout_ms, unsigned int* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return -1;
    }
    return priv->socket->recvu4LSB(timeout_ms, b, xsink);
}

// send HTTP message
int QoreSocketObject::sendHTTPMessage(ExceptionSink* xsink, QoreHashNode* info, const char* method, const char* path,
        const char* http_version, const QoreHashNode* headers, const void* ptr, int size, int source,
        int timeout_ms) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_SEND)) {
        return -1;
    }
    return priv->socket->sendHTTPMessage(xsink, info, method, path, http_version, headers, ptr, size, source,
        timeout_ms);
}

int QoreSocketObject::sendHTTPMessage(ExceptionSink* xsink, QoreHashNode* info, const char* method, const char* path,
        const char* http_version, const QoreHashNode* headers, const QoreStringNode& body, int source,
        int timeout_ms) {
    QoreStringNodeValueHelper tstr(&body, priv->socket->getEncoding(), xsink);
    if (*xsink) {
        return -1;
    }
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_SEND)) {
        return -1;
    }
    return priv->socket->priv->sendHttpMessage(xsink, info, "Socket", "sendHTTPMessage", method, path, http_version,
        headers, *tstr, tstr->c_str(), tstr->size(), nullptr, nullptr, 0, nullptr, source, timeout_ms, &priv->m);
}

int QoreSocketObject::sendHTTPMessageWithCallback(ExceptionSink* xsink, QoreHashNode* info, const char* method,
        const char* path, const char* http_version, const QoreHashNode* headers,
        const ResolvedCallReferenceNode& send_callback, int source, int timeout_ms, bool* aborted) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_SEND)) {
        return -1;
    }
    return priv->socket->priv->sendHttpMessage(xsink, info, "Socket", "sendHTTPMessageWithCallback", method, path,
        http_version, headers, nullptr, nullptr, 0, &send_callback, nullptr, 0, nullptr, source, timeout_ms, &priv->m,
        aborted);
}

// send HTTP response
int QoreSocketObject::sendHTTPResponse(ExceptionSink* xsink, QoreHashNode* info, int code, const char* desc,
        const char* http_version, const QoreHashNode* headers, const void* ptr, size_t size, int source,
        int timeout_ms) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_SEND)) {
        return -1;
    }
    return priv->socket->sendHTTPResponse(xsink, info, code, desc, http_version, headers, ptr, size, source,
        timeout_ms);
}

int QoreSocketObject::sendHTTPResponse(ExceptionSink* xsink, QoreHashNode* info, int code, const char* desc,
        const char* http_version, const QoreHashNode* headers, const QoreStringNode& body, int source,
        int timeout_ms) {
    QoreStringNodeValueHelper tstr(&body, priv->socket->getEncoding(), xsink);
    if (*xsink) {
        return -1;
    }
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_SEND)) {
        return -1;
    }
    return priv->socket->priv->sendHttpResponse(xsink, info, "Socket", "sendHTTPResponse", code, desc, http_version,
        headers, *tstr, tstr->c_str(), tstr->size(), nullptr, nullptr, 0, nullptr, source, timeout_ms, &priv->m);
}

int QoreSocketObject::sendHTTPResponse(ExceptionSink* xsink, QoreHashNode* info, int code, const char* desc,
        const char* http_version, const QoreHashNode* headers, InputStream *input_stream, size_t max_chunked_size,
    const ResolvedCallReferenceNode* trailer_callback, int source, int timeout_ms) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_SEND)) {
        return -1;
    }
    return priv->socket->priv->sendHttpResponse(xsink, info, "Socket", "sendHTTPResponse", code, desc, http_version,
        headers, nullptr, nullptr, 0, nullptr, input_stream, max_chunked_size, trailer_callback, source, timeout_ms,
        &priv->m);
}

int QoreSocketObject::sendHTTPResponseWithCallback(ExceptionSink* xsink, QoreHashNode* info, int code,
        const char* desc, const char* http_version, const QoreHashNode* headers,
    const ResolvedCallReferenceNode& send_callback, int source, int timeout_ms, bool* aborted) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_SEND)) {
        return -1;
    }
    return priv->socket->priv->sendHttpResponse(xsink, info, "Socket", "sendHTTPResponseWithCallback", code, desc,
        http_version, headers, nullptr, nullptr, 0, &send_callback, nullptr, 0, nullptr, source, timeout_ms, &priv->m,
        aborted);
}

// send data in HTTP chunked format
void QoreSocketObject::sendHTTPChunkedBodyFromInputStream(InputStream* is, size_t max_chunked_size,
        const int timeout_ms, const ResolvedCallReferenceNode* trailer_callback, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_SEND)) {
        return;
    }
    return priv->socket->priv->sendHttpChunkedBodyFromInputStream(is, max_chunked_size, timeout_ms, xsink, &priv->m,
        trailer_callback);
}

void QoreSocketObject::sendHTTPChunkedBodyTrailer(const QoreHashNode* headers, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_SEND)) {
        return;
    }
    return priv->socket->priv->sendHttpChunkedBodyTrailer(headers, timeout_ms, xsink);
}

QoreHashNode* QoreSocketObject::readHttpChunk(int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return nullptr;
    }
    return priv->socket->readHttpChunk(timeout_ms, xsink);
}

// receive a binary message in HTTP chunked format
QoreHashNode* QoreSocketObject::readHTTPChunkedBodyBinary(int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return nullptr;
    }
    return priv->socket->readHTTPChunkedBodyBinary(timeout_ms, xsink);
}

// receive a binary message in HTTP chunked format
QoreHashNode* QoreSocketObject::readHTTPChunkedBodyToOutputStream(OutputStream* os, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return nullptr;
    }
    return priv->socket->priv->readHttpChunkedBodyBinary(timeout_ms, xsink, "Socket", QORE_SOURCE_SOCKET, 0, &priv->m, 0, os);
}

// receive a string message in HTTP chunked format
QoreHashNode* QoreSocketObject::readHTTPChunkedBody(int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return nullptr;
    }
    return priv->socket->readHTTPChunkedBody(timeout_ms, xsink);
}

void QoreSocketObject::readHTTPChunkedBodyBinaryWithCallback(const ResolvedCallReferenceNode& recv_callback,
        QoreObject* obj, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return;
    }
    priv->socket->priv->readHttpChunkedBodyBinary(timeout_ms, xsink, "Socket", QORE_SOURCE_SOCKET, &recv_callback,
        &priv->m, obj);
}

// receive a string message in HTTP chunked format
void QoreSocketObject::readHTTPChunkedBodyWithCallback(const ResolvedCallReferenceNode& recv_callback,
        QoreObject* obj, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return;
    }
    priv->socket->priv->readHttpChunkedBody(timeout_ms, xsink, "Socket", QORE_SOURCE_SOCKET, &recv_callback, &priv->m,
        obj);
}

// read and parse HTTP header
AbstractQoreNode* QoreSocketObject::readHTTPHeader(ExceptionSink* xsink, QoreHashNode* info, int timeout_ms) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return nullptr;
    }
    return priv->socket->readHTTPHeader(xsink, info, timeout_ms);
}

QoreStringNode* QoreSocketObject::readHTTPHeaderString(ExceptionSink* xsink, int timeout_ms) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return nullptr;
    }
    return priv->socket->readHTTPHeaderString(xsink, timeout_ms);
}

QoreHashNode* QoreSocketObject::readServerSentEvent(ExceptionSink* xsink, const QoreStringNode* content_encoding,
        int timeout_ms) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink, NB_RECV)) {
        return nullptr;
    }
    return priv->socket->readServerSentEvent(xsink, content_encoding, timeout_ms);
}

int QoreSocketObject::setSendTimeout(int ms) {
    AutoLocker al(priv->m);
    return priv->socket->setSendTimeout(ms);
}

int QoreSocketObject::setRecvTimeout(int ms) {
    AutoLocker al(priv->m);
    return priv->socket->setRecvTimeout(ms);
}

int QoreSocketObject::getSendTimeout() {
    AutoLocker al(priv->m);
    return priv->socket->getSendTimeout();
}

int QoreSocketObject::getRecvTimeout() {
    AutoLocker al(priv->m);
    return priv->socket->getRecvTimeout();
}

int QoreSocketObject::close() {
    AutoLocker al(priv->m);
    return priv->socket->close();
}

int QoreSocketObject::shutdown() {
    AutoLocker al(priv->m);
    return priv->socket->shutdown();
}

int QoreSocketObject::shutdownSSL(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->shutdownSSL(xsink);
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
    AutoLocker al(priv->m);
    return priv->socket->submitHttp2PushPromise(stream_id, path, headers, xsink);
}

int QoreSocketObject::submitHttp2Response(int32_t stream_id, int status_code,
        const QoreHashNode* headers, const void* body, size_t body_len,
        ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    return priv->socket->submitHttp2Response(stream_id, status_code, headers, body, body_len, xsink);
}

int QoreSocketObject::submitHttp2ConnectResponse(int32_t stream_id, int status_code,
        const QoreHashNode* headers, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    return priv->socket->submitHttp2ConnectResponse(stream_id, status_code, headers, xsink);
}

int32_t QoreSocketObject::submitHttp2Request(const QoreHashNode* headers, const void* body,
        size_t body_len, ExceptionSink* xsink, bool streaming) {
    AutoLocker al(priv->m);
    return priv->socket->submitHttp2Request(headers, body, body_len, xsink, streaming);
}

void QoreSocketObject::cancelHttp2Stream(int32_t stream_id, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    priv->socket->cancelHttp2Stream(stream_id, xsink);
}

void QoreSocketObject::setHttp2ConnectProtocolEnabled(bool enable) {
    AutoLocker al(priv->m);
    priv->socket->setHttp2ConnectProtocolEnabled(enable);
}

void QoreSocketObject::setHttp2ActiveStream(int32_t stream_id, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    priv->socket->setHttp2ActiveStream(stream_id, xsink);
}

int32_t QoreSocketObject::getHttp2ActiveStream() const {
    AutoLocker al(priv->m);
    return priv->socket->getHttp2ActiveStream();
}

int QoreSocketObject::sendHttp2StreamData(int32_t stream_id, const BinaryNode* data,
        bool end_stream, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    return priv->socket->sendHttp2StreamData(stream_id, data, end_stream, xsink);
}

BinaryNode* QoreSocketObject::readHttp2StreamData(int32_t stream_id, size_t max_bytes, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    return priv->socket->readHttp2StreamData(stream_id, max_bytes, xsink);
}

int QoreSocketObject::sendHttp2Trailers(int32_t stream_id, const QoreHashNode* trailers,
        ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    return priv->socket->sendHttp2Trailers(stream_id, trailers, xsink);
}

int QoreSocketObject::submitHttp2StreamingResponseHeaders(int32_t stream_id, int status_code,
        const QoreHashNode* headers, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    return priv->socket->submitHttp2StreamingResponseHeaders(stream_id, status_code, headers, xsink);
}

int QoreSocketObject::submitHttp2StreamingResponseWithStream(int32_t stream_id, int status_code,
        const QoreHashNode* headers, InputStream* body, ExceptionSink* xsink) {
    // C++ vtable is the sole authority on I/O thread eligibility.
    // Return 1 (not accepted) so the caller can fall back to handler-thread streaming.
    // No exception — the caller handles the fallback path.
    if (!body->isIoThreadSafe()) {
        return 1;
    }

    AutoLocker al(priv->m);
    Http2Session* session = priv->socket->priv->h2_session.get();
    if (!session) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session available");
        return -1;
    }

    // Submit response headers without END_STREAM
    int rv = priv->socket->submitHttp2StreamingResponseHeaders(stream_id, status_code, headers, xsink);
    if (rv) {
        return rv;
    }

    // Transfer ownership of InputStream to the session for I/O thread reading
    // The handler thread unassigns before calling; I/O thread will reassign on first read
    body->unassignThread(xsink);
    if (*xsink) {
        return -1;
    }

    session->setStreamInputStream(stream_id, body, xsink);
    return *xsink ? -1 : 0;
}

// NOTE: readHttp2StreamDataBlock does NOT acquire priv->m because it blocks
// during I/O (waiting for HTTP/2 stream data with timeout).  Holding the
// object mutex during a blocking wait would prevent other threads from
// performing any socket operation (write, flush, cleanup) on this socket.
// The caller must ensure exclusive socket access (which is the case for
// server handler threads that own the connection).  Thread safety for the
// HTTP/2 session internals is provided by Http2Session's own recursive mutex.
// No concurrent receiveData() calls can occur on the same session because the
// server connection model is single-threaded per connection: the handler thread
// that calls readHttp2StreamDataBlock() is the same thread that drives the
// HTTP/2 session.  flushHttp2()/sendPendingDataBlocking() only sends outgoing
// frames and does not call receiveData(), so there is no nghttp2 reentrancy risk.
BinaryNode* QoreSocketObject::readHttp2StreamDataBlock(int32_t stream_id, int timeout_ms,
        ExceptionSink* xsink) {
    return priv->socket->readHttp2StreamDataBlock(stream_id, timeout_ms, xsink);
}

bool QoreSocketObject::isHttp2StreamComplete(int32_t stream_id) const {
    AutoLocker al(priv->m);
    return priv->socket->isHttp2StreamComplete(stream_id);
}

bool QoreSocketObject::isHttp2StreamClosed(int32_t stream_id) const {
    AutoLocker al(priv->m);
    return priv->socket->isHttp2StreamClosed(stream_id);
}

bool QoreSocketObject::isHttp2StreamRemoteClosed(int32_t stream_id) const {
    AutoLocker al(priv->m);
    return priv->socket->isHttp2StreamRemoteClosed(stream_id);
}

int QoreSocketObject::flushHttp2(int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    return priv->socket->flushHttp2(timeout_ms, xsink);
}

void QoreSocketObject::cleanupHttp2Stream(int32_t stream_id) {
    AutoLocker al(priv->m);
    priv->socket->cleanupHttp2Stream(stream_id);
}

int QoreSocketObject::resetHttp2Stream(int32_t stream_id, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    return priv->socket->resetHttp2Stream(stream_id, xsink);
}

int QoreSocketObject::waitForHttp2StreamDrain(int32_t stream_id, int timeout_ms) {
    // Do NOT hold priv->m while waiting — the I/O thread needs priv->m to
    // call sendPendingData().  The CV wait only uses Http2Session's internal
    // drain_mtx_ which is independent of the socket lock.
    Http2SessionPtr h2;
    {
        AutoLocker al(priv->m);
        h2 = priv->socket->priv->h2_session;
    }
    if (!h2) {
        return -1;
    }
    return h2->waitForStreamDrain(stream_id, timeout_ms);
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

void QoreSocketObject::setEncoding(const QoreEncoding* id) {
    priv->socket->setEncoding(id);
}

const QoreEncoding* QoreSocketObject::getEncoding() const {
    return priv->socket->getEncoding();
}

bool QoreSocketObject::isDataAvailable(ExceptionSink* xsink, int timeout_ms) {
    AutoLocker al(priv->m);
    return priv->socket->isDataAvailable(xsink, timeout_ms);
}

bool QoreSocketObject::isWriteFinished(ExceptionSink* xsink, int timeout_ms) {
    AutoLocker al(priv->m);
    return priv->socket->isWriteFinished(xsink, timeout_ms);
}

bool QoreSocketObject::isOpen() const {
    return priv->socket->isOpen();
}

int QoreSocketObject::connectINETSSL(ExceptionSink* xsink, const char* host, int port, int timeout_ms) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->connectINETSSL(xsink, host, port, timeout_ms, priv->cert, priv->pk);
}

int QoreSocketObject::connectINET2SSL(ExceptionSink* xsink, const char* name, const char* service, int family,
        int sock_type, int protocol, int timeout_ms) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->connectINET2SSL(xsink, name, service, family, sock_type, protocol, timeout_ms,
        priv->cert, priv->pk);
}

int QoreSocketObject::connectUNIXSSL(ExceptionSink* xsink, const char* p, int sock_type, int protocol) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->connectUNIXSSL(xsink, p, sock_type, protocol, priv->cert, priv->pk);
}

int QoreSocketObject::connectSSL(ExceptionSink* xsink, const char* name, int timeout_ms) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->connectSSL(xsink, name, timeout_ms, priv->cert, priv->pk);
}

QoreSocketObject* QoreSocketObject::accept(SocketSource* source, ExceptionSink* xsink) {
    QoreSocket* s;
    {
        AutoLocker al(priv->m);
        if (priv->checkNonBlock(xsink)) {
            return nullptr;
        }
        s = priv->socket->accept(source, xsink);
    }
    return s ? new QoreSocketObject(s, priv->cert ? priv->cert->certRefSelf() : nullptr,
        priv->pk ? priv->pk->pkRefSelf() : nullptr) : nullptr;
}

QoreSocketObject* QoreSocketObject::acceptSSL(ExceptionSink* xsink, SocketSource* source) {
    QoreSocket* s;
    {
        AutoLocker al(priv->m);
        if (priv->checkNonBlock(xsink)) {
            return nullptr;
        }
        s = priv->socket->acceptSSL(xsink, source, priv->cert, priv->pk);
    }
    return s
        ? new QoreSocketObject(s, priv->cert ? priv->cert->certRefSelf() : nullptr,
            priv->pk ? priv->pk->pkRefSelf() : nullptr)
        : nullptr;
}

QoreSocketObject* QoreSocketObject::accept(int timeout_ms, ExceptionSink* xsink) {
    QoreSocket* s;
    {
        AutoLocker al(priv->m);
        if (priv->checkNonBlock(xsink)) {
            return nullptr;
        }
        s = priv->socket->accept(timeout_ms, xsink);
    }
    return s
        ? new QoreSocketObject(s, priv->cert ? priv->cert->certRefSelf() : nullptr,
            priv->pk ? priv->pk->pkRefSelf() : nullptr)
        : nullptr;
}

QoreSocketObject* QoreSocketObject::acceptSSL(ExceptionSink* xsink, int timeout_ms) {
    QoreSocket* s;
    {
        AutoLocker al(priv->m);
        if (priv->checkNonBlock(xsink)) {
            return nullptr;
        }
        s = priv->socket->acceptSSL(xsink, timeout_ms, priv->cert, priv->pk);
    }
    return s
        ? new QoreSocketObject(s, priv->cert ? priv->cert->certRefSelf() : nullptr,
            priv->pk ? priv->pk->pkRefSelf() : nullptr)
        : nullptr;
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

void QoreSocketObject::upgradeClientToSSL(ExceptionSink* xsink, int timeout_ms) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return;
    }
    priv->socket->upgradeClientToSSL(xsink, timeout_ms, priv->cert, priv->pk);
}

void QoreSocketObject::upgradeServerToSSL(ExceptionSink* xsink, int timeout_ms) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return;
    }
    priv->socket->upgradeServerToSSL(xsink, timeout_ms, priv->cert, priv->pk);
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
    AutoLocker al(priv->m);
    return priv->socket->setNoDelay(nodelay);
}

int QoreSocketObject::getNoDelay() {
    AutoLocker al(priv->m);
    return priv->socket->getNoDelay();
}

QoreHashNode* QoreSocketObject::getPeerInfo(ExceptionSink* xsink, bool host_lookup) const {
    AutoLocker al(priv->m);
    return priv->socket->getPeerInfo(xsink, host_lookup);
}

QoreHashNode* QoreSocketObject::getSocketInfo(ExceptionSink* xsink, bool host_lookup) const {
    AutoLocker al(priv->m);
    return priv->socket->getSocketInfo(xsink, host_lookup);
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
    AutoLocker al(priv->m);
    // Get the first QUIC session (client connections have exactly one)
    std::shared_ptr<QuicSession> session;
    {
        qore_socket_private* sp = qore_socket_private::get(*priv->socket);
        AutoLocker al2(sp->quic_sessions_lock);
        if (!sp->quic_sessions.empty()) {
            session = sp->quic_sessions.begin()->second;
        }
    }
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
    AutoLocker al(priv->m);
    // Get the first QUIC session (client connections have exactly one)
    std::shared_ptr<QuicSession> session;
    {
        qore_socket_private* sp = qore_socket_private::get(*priv->socket);
        AutoLocker al2(sp->quic_sessions_lock);
        if (!sp->quic_sessions.empty()) {
            session = sp->quic_sessions.begin()->second;
        }
    }
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
    // Brief lock for session lookup; release before calling session method
    std::shared_ptr<QuicSession> session;
    {
        AutoLocker al(priv->m);
        qore_socket_private* sp = qore_socket_private::get(*priv->socket);
        if (!sp->quic_sessions.empty()) {
            session = sp->quic_sessions.begin()->second;
        }
    }
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no active QUIC session on this socket");
        return -1;
    }

    return session->sendStreamData(stream_id, data, len, end_stream, xsink);
}

int QoreSocketObject::waitForQuicClientStreamDrain(int64_t stream_id, int timeout_ms,
        ExceptionSink* xsink) {
    // Brief lock for session lookup; release before blocking wait
    std::shared_ptr<QuicSession> session;
    {
        AutoLocker al(priv->m);
        qore_socket_private* sp = qore_socket_private::get(*priv->socket);
        if (!sp->quic_sessions.empty()) {
            session = sp->quic_sessions.begin()->second;
        }
    }
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no active QUIC session on this socket");
        return -1;
    }

    return session->waitForStreamDrain(stream_id, timeout_ms);
}

void QoreSocketObject::cancelQuicStream(int64_t session_id, int64_t stream_id, ExceptionSink* xsink) {
    // Brief lock for session lookup; release before calling session method to avoid
    // contention with the I/O thread's continuePoll() which also holds priv->m
    std::shared_ptr<QuicSession> session;
    {
        AutoLocker al(priv->m);
        qore_socket_private* sp = qore_socket_private::get(*priv->socket);
        session = sp->getQuicSession(session_id);
    }
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return;
    }

    session->cancelStream(stream_id, NGHTTP3_H3_REQUEST_CANCELLED, xsink);
}

int QoreSocketObject::submitQuicResponse(int64_t session_id, int64_t stream_id, int status_code,
        const QoreHashNode* headers, const void* body, size_t body_len, ExceptionSink* xsink) {
    // Thread safety: priv->m serializes concurrent calls from multiple handler
    // threads.  The lock is held while building headers and calling
    // session->submitResponse(), which only mutates QuicSession internal state
    // (body_data_, nghttp3 submit).  Actual packet I/O happens later when the
    // poll operation calls writePackets()/sendPendingPackets().
    AutoLocker al(priv->m);
    // Get the QUIC session by session_id
    qore_socket_private* sp = qore_socket_private::get(*priv->socket);
    std::shared_ptr<QuicSession> session = sp->getQuicSession(session_id);
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
    AutoLocker al(priv->m);
    qore_socket_private* sp = qore_socket_private::get(*priv->socket);
    std::shared_ptr<QuicSession> session = sp->getQuicSession(session_id);
    if (!session) {
        xsink->raiseException("QUIC-SESSION-ERROR", "session %lld not found",
                              (long long)session_id);
        return;
    }
    session->submitShutdownNotice(xsink);
}

void QoreSocketObject::submitQuicShutdown(int64_t session_id, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    qore_socket_private* sp = qore_socket_private::get(*priv->socket);
    std::shared_ptr<QuicSession> session = sp->getQuicSession(session_id);
    if (!session) {
        xsink->raiseException("QUIC-SESSION-ERROR", "session %lld not found",
                              (long long)session_id);
        return;
    }
    session->submitShutdown(xsink);
}

QoreHashNode* QoreSocketObject::getQuicSessionGoawayState(int64_t session_id, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    qore_socket_private* sp = qore_socket_private::get(*priv->socket);
    std::shared_ptr<QuicSession> session = sp->getQuicSession(session_id);
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
    AutoLocker al(priv->m);
    qore_socket_private* sp = qore_socket_private::get(*priv->socket);
    std::shared_ptr<QuicSession> session = sp->getQuicSession(session_id);
    if (!session) {
        // Return false rather than raising an exception for a lightweight check;
        // the session may have been cleaned up between checks
        return false;
    }
    return session->isGoawayReceived();
}

QoreObject* QoreSocketObject::getQuicPeerCertificate(int64_t session_id, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    qore_socket_private* sp = qore_socket_private::get(*priv->socket);
    std::shared_ptr<QuicSession> session = sp->getQuicSession(session_id);
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
    // Brief lock for session lookup; release before calling session method to avoid
    // contention with the I/O thread's continuePoll() which also holds priv->m
    std::shared_ptr<QuicSession> session;
    {
        AutoLocker al(priv->m);
        qore_socket_private* sp = qore_socket_private::get(*priv->socket);
        session = sp->getQuicSession(session_id);
    }
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
    // Brief lock for session lookup; release before calling session method to avoid
    // contention with the I/O thread's continuePoll() which also holds priv->m
    std::shared_ptr<QuicSession> session;
    {
        AutoLocker al(priv->m);
        qore_socket_private* sp = qore_socket_private::get(*priv->socket);
        session = sp->getQuicSession(session_id);
    }
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return -1;
    }

    return session->sendStreamData(stream_id, data, len, end_stream, xsink);
}

void QoreSocketObject::setQuicStreamInputStream(int64_t session_id, int64_t stream_id,
        InputStream* body, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    qore_socket_private* sp = qore_socket_private::get(*priv->socket);
    std::shared_ptr<QuicSession> session = sp->getQuicSession(session_id);
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
    // Look up the session with a brief lock, then release it before waiting.
    // waitForStreamDrain() only acquires QuicSession::mtx_, not the socket lock,
    // so there is no deadlock risk with I/O threads that hold priv->m.
    std::shared_ptr<QuicSession> session;
    {
        AutoLocker al(priv->m);
        qore_socket_private* sp = qore_socket_private::get(*priv->socket);
        session = sp->getQuicSession(session_id);
    }
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return -1;
    }
    return session->waitForStreamDrain(stream_id, timeout_ms);
}

int QoreSocketObject::submitQuicConnectResponse(int64_t session_id, int64_t stream_id,
        int status_code, const QoreHashNode* headers, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    qore_socket_private* sp = qore_socket_private::get(*priv->socket);
    std::shared_ptr<QuicSession> session = sp->getQuicSession(session_id);
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
    AutoLocker al(priv->m);
    qore_socket_private* sp = qore_socket_private::get(*priv->socket);
    std::shared_ptr<QuicSession> session = sp->getQuicSession(session_id);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return QoreValue();
    }

    return session->readConnectStreamData(stream_id, xsink);
}

void QoreSocketObject::registerQuicConnectStreamQueue(int64_t session_id, int64_t stream_id,
        Queue* queue, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    qore_socket_private* sp = qore_socket_private::get(*priv->socket);
    std::shared_ptr<QuicSession> session = sp->getQuicSession(session_id);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return;
    }
    session->registerConnectStreamQueue(stream_id, queue);
}

void QoreSocketObject::deregisterQuicConnectStreamQueue(int64_t session_id, int64_t stream_id,
        ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    qore_socket_private* sp = qore_socket_private::get(*priv->socket);
    std::shared_ptr<QuicSession> session = sp->getQuicSession(session_id);
    if (!session) {
        return;  // silently ignore — session may already be gone during cleanup
    }
    session->deregisterConnectStreamQueue(stream_id);
}

BinaryNode* QoreSocketObject::readQuicStreamDataBlock(int64_t session_id, int64_t stream_id,
        int timeout_ms, ExceptionSink* xsink) {
    // Get the session WITHOUT holding the socket lock — the session is reference-counted
    // and we need to avoid holding the socket lock while blocking on the CV
    std::shared_ptr<QuicSession> session;
    {
        AutoLocker al(priv->m);
        qore_socket_private* sp = qore_socket_private::get(*priv->socket);
        session = sp->getQuicSession(session_id);
    }
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return nullptr;
    }
    // Use a deadline to avoid timeout reset when data arrives incrementally
    int64 deadline_ms = timeout_ms >= 0
        ? q_clock_getmillis() + timeout_ms
        : -1;

    // Get per-stream notifier for targeted wakeup (avoids thundering herd)
    std::shared_ptr<StreamNotifier> notifier = session->getStreamNotifier(stream_id);

    while (true) {
        // Check if session has been torn down (abort)
        if (session->isMarkedClosed()) {
            xsink->raiseException("QUIC-ERROR",
                "QUIC session closed during stream read (session %lld, stream %lld)",
                (long long)session_id, (long long)stream_id);
            return nullptr;
        }

        // 1. Atomically check stream buffer AND completion status under one lock
        // This eliminates the TOCTOU race where data could arrive between separate
        // takeStreamData() and isStreamComplete() calls
        bool complete = false;
        QoreValue data = session->takeStreamData(stream_id, complete);
        if (data.getType() == NT_BINARY) {
            return data.get<BinaryNode>();
        }
        if (complete) {
            return nullptr;
        }

        // 3. Calculate remaining timeout from deadline
        int remaining_ms;
        if (deadline_ms >= 0) {
            remaining_ms = (int)(deadline_ms - q_clock_getmillis());
            if (remaining_ms <= 0) {
                // Timeout — raise exception so callers can distinguish from stream completion
                xsink->raiseException("QUIC-STREAM-TIMEOUT",
                    "timeout reading QUIC stream data (session %lld, stream %lld)",
                    (long long)session_id, (long long)stream_id);
                return nullptr;
            }
        } else {
            remaining_ms = -1;
        }

        // 4. Wait for stream data or completion — per-stream notifier avoids thundering herd
        if (notifier) {
            notifier->wait(remaining_ms);
        } else {
            session->waitForStreamData(remaining_ms);
        }
    }
}

bool QoreSocketObject::isQuicStreamComplete(int64_t session_id, int64_t stream_id) const {
    AutoLocker al(priv->m);
    qore_socket_private* sp = qore_socket_private::get(*priv->socket);
    std::shared_ptr<QuicSession> session = sp->getQuicSession(session_id);
    if (!session) {
        return true;  // Session not found, treat as complete
    }
    return session->isStreamComplete(stream_id);
}

void QoreSocketObject::cleanupQuicStream(int64_t session_id, int64_t stream_id,
        ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    qore_socket_private* sp = qore_socket_private::get(*priv->socket);
    std::shared_ptr<QuicSession> session = sp->getQuicSession(session_id);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return;
    }
    session->cleanupStream(stream_id);
}

int QoreSocketObject::resetQuicStream(int64_t session_id, int64_t stream_id,
        ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    qore_socket_private* sp = qore_socket_private::get(*priv->socket);
    std::shared_ptr<QuicSession> session = sp->getQuicSession(session_id);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return -1;
    }
    return session->resetStream(stream_id);
}

int QoreSocketObject::submitQuicDatagram(int64_t session_id, int64_t stream_id,
        const BinaryNode* data, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    qore_socket_private* sp = qore_socket_private::get(*priv->socket);
    std::shared_ptr<QuicSession> session = sp->getQuicSession(session_id);
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
    // NOTE: readDatagram uses its own mutex (datagram_mutex_), not the session mutex,
    // so we don't need to hold priv->m for the blocking read (which would deadlock
    // the I/O thread). We only hold priv->m briefly to look up the session.
    std::shared_ptr<QuicSession> session;
    {
        AutoLocker al(priv->m);
        qore_socket_private* sp = qore_socket_private::get(*priv->socket);
        session = sp->getQuicSession(session_id);
    }
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return QoreValue();
    }
    return session->readDatagram(stream_id, timeout_ms, xsink);
}

int64_t QoreSocketObject::getQuicMaxDatagramSize(int64_t session_id, int64_t stream_id,
        ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    qore_socket_private* sp = qore_socket_private::get(*priv->socket);
    std::shared_ptr<QuicSession> session = sp->getQuicSession(session_id);
    if (!session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return 0;
    }
    return static_cast<int64_t>(session->getMaxDatagramPayloadSize(stream_id));
}

bool QoreSocketObject::isQuicDatagramSupported(int64_t session_id, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    qore_socket_private* sp = qore_socket_private::get(*priv->socket);
    std::shared_ptr<QuicSession> session = sp->getQuicSession(session_id);
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

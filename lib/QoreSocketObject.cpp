/*
    QoreSocketObject.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2023 Qore Technologies, s.r.o.

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
#include "qore/intern/qore_socket_private.h"
#include "qore/intern/QC_Socket.h"
#include "qore/intern/QC_SSLCertificate.h"
#include "qore/intern/QC_SSLPrivateKey.h"

QoreSocketObject::QoreSocketObject(QoreSocket* s, QoreSSLCertificate* cert, QoreSSLPrivateKey* pk)
        : priv(new my_socket_priv(s, cert, pk)) {
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

AbstractPollState* QoreSocketObject::startRecv(ExceptionSink* xsink, size_t size) {
    AutoLocker al(priv->m);
    return priv->socket->startRecv(xsink, size);
}

AbstractPollState* QoreSocketObject::startRecvUntilBytes(ExceptionSink* xsink, const char* pattern, size_t size) {
    AutoLocker al(priv->m);
    return priv->socket->startRecvUntilBytes(xsink, pattern, size);
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
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->send(buf, size, timeout_ms, xsink);
}

// send a null-terminated string
int QoreSocketObject::send(const QoreStringNode& msg, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->send(msg, timeout_ms, xsink);
}

// send a binary object
int QoreSocketObject::send(const BinaryNode* b, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
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
    if (priv->checkNonBlock(xsink)) {
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
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->sendi1(b, timeout_ms, xsink);
}

int QoreSocketObject::sendi2(short b, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->sendi2(b, timeout_ms, xsink);
}

int QoreSocketObject::sendi4(int b, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->sendi4(b, timeout_ms, xsink);
}

int QoreSocketObject::sendi8(int64 b, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->sendi8(b, timeout_ms, xsink);
}

int QoreSocketObject::sendi2LSB(short b, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->sendi2LSB(b, timeout_ms, xsink);
}

int QoreSocketObject::sendi4LSB(int b, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->sendi4LSB(b, timeout_ms, xsink);
}

int QoreSocketObject::sendi8LSB(int64 b, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->sendi8LSB(b, timeout_ms, xsink);
}

// receive a packet of bytes as a string
QoreStringNode* QoreSocketObject::recv(int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return nullptr;
    }
    return priv->socket->recv(timeout_ms, xsink);
}

// receive a certain number of bytes as a string
QoreStringNode* QoreSocketObject::recv(qore_offset_t bufsize, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return nullptr;
    }
    return priv->socket->recv(bufsize, timeout_ms, xsink);
}

// receive a packet of bytes as a binary
BinaryNode* QoreSocketObject::recvBinary(int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return nullptr;
    }
    return priv->socket->recvBinary(timeout_ms, xsink);
}

// receive a certain number of bytes as a binary object
BinaryNode* QoreSocketObject::recvBinary(int bufsize, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return nullptr;
    }
    return priv->socket->recvBinary(bufsize, timeout_ms, xsink);
}

void QoreSocketObject::recvToOutputStream(OutputStream *os, int64 size, int64 timeout_ms, ExceptionSink *xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
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
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->recvi1(timeout_ms, b, xsink);
}

int64 QoreSocketObject::recvi2(int timeout_ms, short* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->recvi2(timeout_ms, b, xsink);
}

int64 QoreSocketObject::recvi4(int timeout_ms, int* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->recvi4(timeout_ms, b, xsink);
}

int64 QoreSocketObject::recvi8(int timeout_ms, int64* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->recvi8(timeout_ms, b, xsink);
}

int64 QoreSocketObject::recvi2LSB(int timeout_ms, short* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->recvi2LSB(timeout_ms, b, xsink);
}

int64 QoreSocketObject::recvi4LSB(int timeout_ms, int* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->recvi4LSB(timeout_ms, b, xsink);
}

int64 QoreSocketObject::recvi8LSB(int timeout_ms, int64* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->recvi8LSB(timeout_ms, b, xsink);
}

// receive integers and convert from network byte order
int64 QoreSocketObject::recvu1(int timeout_ms, unsigned char* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->recvu1(timeout_ms, b, xsink);
}

int64 QoreSocketObject::recvu2(int timeout_ms, unsigned short* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->recvu2(timeout_ms, b, xsink);
}

int64 QoreSocketObject::recvu4(int timeout_ms, unsigned int* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->recvu4(timeout_ms, b, xsink);
}

int64 QoreSocketObject::recvu2LSB(int timeout_ms, unsigned short* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->recvu2LSB(timeout_ms, b, xsink);
}

int64 QoreSocketObject::recvu4LSB(int timeout_ms, unsigned int* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->recvu4LSB(timeout_ms, b, xsink);
}

// send HTTP message
int QoreSocketObject::sendHTTPMessage(ExceptionSink* xsink, QoreHashNode* info, const char* method, const char* path,
        const char* http_version, const QoreHashNode* headers, const void* ptr, int size, int source,
        int timeout_ms) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
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
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->priv->sendHttpMessage(xsink, info, "Socket", "sendHTTPMessage", method, path, http_version,
        headers, *tstr, tstr->c_str(), tstr->size(), nullptr, nullptr, 0, nullptr, source, timeout_ms, &priv->m);
}

int QoreSocketObject::sendHTTPMessageWithCallback(ExceptionSink* xsink, QoreHashNode* info, const char* method,
        const char* path, const char* http_version, const QoreHashNode* headers,
        const ResolvedCallReferenceNode& send_callback, int source, int timeout_ms, bool* aborted) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
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
    if (priv->checkNonBlock(xsink)) {
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
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->priv->sendHttpResponse(xsink, info, "Socket", "sendHTTPResponse", code, desc, http_version,
        headers, *tstr, tstr->c_str(), tstr->size(), nullptr, nullptr, 0, nullptr, source, timeout_ms, &priv->m);
}

int QoreSocketObject::sendHTTPResponse(ExceptionSink* xsink, QoreHashNode* info, int code, const char* desc,
        const char* http_version, const QoreHashNode* headers, InputStream *input_stream, size_t max_chunked_size,
    const ResolvedCallReferenceNode* trailer_callback, int source, int timeout_ms) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
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
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }
    return priv->socket->priv->sendHttpResponse(xsink, info, "Socket", "sendHTTPResponseWithCallback", code, desc,
        http_version, headers, nullptr, nullptr, 0, &send_callback, nullptr, 0, nullptr, source, timeout_ms, &priv->m,
        aborted);
}

// send data in HTTP chunked format
void QoreSocketObject::sendHTTPChunkedBodyFromInputStream(InputStream *is, size_t max_chunked_size,
        const int timeout_ms, const ResolvedCallReferenceNode* trailer_callback, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return;
    }
    return priv->socket->priv->sendHttpChunkedBodyFromInputStream(is, max_chunked_size, timeout_ms, xsink, &priv->m,
        trailer_callback);
}

void QoreSocketObject::sendHTTPChunkedBodyTrailer(const QoreHashNode *headers, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return;
    }
    return priv->socket->priv->sendHttpChunkedBodyTrailer(headers, timeout_ms, xsink);
}

// receive a binary message in HTTP chunked format
QoreHashNode* QoreSocketObject::readHTTPChunkedBodyBinary(int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return nullptr;
    }
    return priv->socket->readHTTPChunkedBodyBinary(timeout_ms, xsink);
}

// receive a binary message in HTTP chunked format
QoreHashNode* QoreSocketObject::readHTTPChunkedBodyToOutputStream(OutputStream *os, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return nullptr;
    }
    return priv->socket->priv->readHttpChunkedBodyBinary(timeout_ms, xsink, "Socket", QORE_SOURCE_SOCKET, 0, &priv->m, 0, os);
}

// receive a string message in HTTP chunked format
QoreHashNode* QoreSocketObject::readHTTPChunkedBody(int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return nullptr;
    }
    return priv->socket->readHTTPChunkedBody(timeout_ms, xsink);
}

void QoreSocketObject::readHTTPChunkedBodyBinaryWithCallback(const ResolvedCallReferenceNode& recv_callback,
        QoreObject* obj, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return;
    }
    priv->socket->priv->readHttpChunkedBodyBinary(timeout_ms, xsink, "Socket", QORE_SOURCE_SOCKET, &recv_callback,
        &priv->m, obj);
}

// receive a string message in HTTP chunked format
void QoreSocketObject::readHTTPChunkedBodyWithCallback(const ResolvedCallReferenceNode& recv_callback,
        QoreObject* obj, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return;
    }
    priv->socket->priv->readHttpChunkedBody(timeout_ms, xsink, "Socket", QORE_SOURCE_SOCKET, &recv_callback, &priv->m,
        obj);
}

// read and parse HTTP header
AbstractQoreNode* QoreSocketObject::readHTTPHeader(ExceptionSink* xsink, QoreHashNode* info, int timeout_ms) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return nullptr;
    }
    return priv->socket->readHTTPHeader(xsink, info, timeout_ms);
}

QoreStringNode* QoreSocketObject::readHTTPHeaderString(ExceptionSink* xsink, int timeout_ms) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return nullptr;
    }
    return priv->socket->readHTTPHeaderString(xsink, timeout_ms);
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

long QoreSocketObject::verifyPeerCertificate() {
    AutoLocker al(priv->m);
    return priv->socket->verifyPeerCertificate();
}

int QoreSocketObject::getPollableDescriptor() const {
    return priv->socket->getSocket();
}

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

int QoreSocketObject::setNonBlock(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    return priv->setNonBlock(xsink);
}

void QoreSocketObject::clearNonBlock() {
    AutoLocker al(priv->m);
    priv->clearNonBlock();
}

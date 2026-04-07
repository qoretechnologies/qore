/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SocketPollOperation.h

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

#ifndef _QORE_SOCKETPOLLOPERATION_H

#define _QORE_SOCKETPOLLOPERATION_H

#include <qore/SocketPollOperationBase.h>
#include <qore/QoreSocketObject.h>
#include <qore/AbstractPollState.h>
#include <qore/ReferenceHolder.h>
#include <qore/AbstractQoreNode.h>

#include <memory>
#include <string>

/** @file SocketPollOperation.h
    Public header exporting the inner poll operation building blocks for
    non-blocking socket I/O.  External modules can compose these operations
    to implement protocol-specific poll pipelines (e.g. HTTP client,
    WebSocket, custom framing protocols).

    @par Exported operations
    - SocketPollSocketOperationBase — intermediate base owning a socket + poll state
    - SocketConnectPollOperation — TCP (+ optional TLS) connect
    - SocketSendPollOperation — non-blocking send (string or binary)
    - SocketRecvPollOperationBase — base for recv operations
    - SocketRecvPollOperation — recv exactly N bytes
    - SocketRecvDataPollOperation — recv available data
    - SocketRecvUntilBytesPollOperation — recv until a byte pattern
    - SocketUpgradeClientSslPollOperation — client-side TLS handshake
    - SocketReadHttpHeaderPollOperation — read HTTP response/request headers
    - SocketReadHttpBodyPollOperation — read HTTP response body (Content-Length / chunked / close)

    @par NOT exported (stay internal)
    - SocketAcceptPollOperation (server-side accept)
    - SocketHttp2* (HTTP/2 protocol)
    - SocketQuic* (QUIC / HTTP/3)
    - SocketSendAndReadHeaderPollOperation (HTTP server keep-alive)
    - SocketSendStreamAndReadHeaderPollOperation
    - SocketRecvFromPollOperation (UDP)
    - SocketSendToPollOperation (UDP)

    @since %Qore 2.3
*/

//! Connection poll goals
///@{
constexpr int SPG_CONNECT = 1;
constexpr int SPG_CONNECT_SSL = 2;
///@}

//! Connection/send/recv poll states
///@{
constexpr int SPS_NONE = 0;
constexpr int SPS_CONNECTING = 1;
constexpr int SPS_CONNECTING_SSL = 2;
constexpr int SPS_CONNECTED = 3;
///@}

//! Intermediate base class that owns a socket reference and an AbstractPollState
/** Provides the abort() logic that clears non-blocking state and optionally
    closes the socket.  All concrete socket poll operations that work on a
    connected socket inherit from this class.

    @since %Qore 2.3
*/
class SocketPollSocketOperationBase : public SocketPollOperationBase {
public:
    //! Creates the operation with a QoreObject self reference (for Qore-space ops)
    DLLEXPORT SocketPollSocketOperationBase(QoreObject* self);

    //! Creates the operation with a referenced socket
    DLLEXPORT SocketPollSocketOperationBase(QoreSocketObject* sock);

    //! Creates the operation with a referenced socket and specific direction flags
    /** @param sock the socket (caller must have ref'd it)
        @param direction bitmask of NB_SEND, NB_RECV, NB_CONNECT
    */
    DLLEXPORT SocketPollSocketOperationBase(QoreSocketObject* sock, unsigned direction);

    DLLEXPORT ~SocketPollSocketOperationBase();

    //! Aborts the operation: resets poll_state, clears non-block, optionally closes socket
    /** @note Out-of-line because the implementation accesses socket internals.
    */
    DLLEXPORT virtual void abort(ExceptionSink* xsink) override;

protected:
    std::unique_ptr<AbstractPollState> poll_state;
    QoreSocketObject* sock = nullptr;
    int state = SPS_NONE;
    bool set_non_block = false;
    //! Direction flags for this operation (NB_SEND, NB_RECV, or NB_ALL)
    unsigned non_block_direction = NB_ALL;

    //! Returns true if abort() should close the socket (default: true)
    DLLEXPORT virtual bool abortNeedsClose() const;
};

//! Non-blocking TCP connect (with optional TLS upgrade)
/** State machine: none -> connecting -> [connecting-ssl ->] connected

    @since %Qore 2.3
*/
class SocketConnectPollOperation : public SocketPollSocketOperationBase {
public:
    //! Creates the connect poll operation
    /** @param xsink exception sink
        @param ssl if true, perform TLS handshake after TCP connect
        @param target connection target (host:port or path)
        @param sock the socket (will be ref'd)
    */
    DLLEXPORT SocketConnectPollOperation(ExceptionSink* xsink, bool ssl, const char* target,
            QoreSocketObject* sock);

    //! Dereferences the operation; clears non-block and deref's the socket on last ref
    DLLEXPORT void deref(ExceptionSink* xsink);

    DLLEXPORT virtual bool goalReached() const override;

    DLLEXPORT virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override;

protected:
    //! Called in the constructor before starting the connect
    /** Subclasses can override to perform pre-connection validation.
        @return 0 on success, non-zero to abort construction
    */
    DLLEXPORT virtual int preVerify(ExceptionSink* xsink);

    //! Called when the TCP connection is established (before TLS)
    DLLEXPORT virtual void connected();

    //! Initiates the TLS handshake after TCP connect
    DLLEXPORT int startSslConnect(ExceptionSink* xsink);

    DLLEXPORT virtual const char* getStateImpl() const override;

    DLLEXPORT int checkContinuePoll(ExceptionSink* xsink);

private:
    std::string target;
    int sgoal = 0;
};

//! Non-blocking send operation (string or binary data)
/** State machine: sending -> sent

    @since %Qore 2.3
*/
class SocketSendPollOperation : public SocketPollSocketOperationBase {
public:
    //! Creates a send operation for string data
    /** @param xsink exception sink
        @param data the string to send (must be passed already referenced)
        @param sock the socket (will be ref'd)
    */
    DLLEXPORT SocketSendPollOperation(ExceptionSink* xsink, QoreStringNode* data, QoreSocketObject* sock);

    //! Creates a send operation for binary data
    /** @param xsink exception sink
        @param data the binary data to send (must be passed already referenced)
        @param sock the socket (will be ref'd)
    */
    DLLEXPORT SocketSendPollOperation(ExceptionSink* xsink, BinaryNode* data, QoreSocketObject* sock);

    //! Dereferences the operation; clears non-block send flag and deref's the socket on last ref
    DLLEXPORT void deref(ExceptionSink* xsink);

    DLLEXPORT virtual bool goalReached() const override;

    DLLEXPORT virtual const char* getStateImpl() const override;

    DLLEXPORT virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override;

private:
    SimpleRefHolder<SimpleValueQoreNode> data;
    const char* buf;
    size_t size;
    bool sent = false;

    DLLEXPORT virtual bool abortNeedsClose() const override;
};

//! Base class for non-blocking receive operations
/** State machine: receiving -> received

    Subclasses provide the specific receive strategy (fixed size, available
    data, or pattern-terminated).

    @since %Qore 2.3
*/
class SocketRecvPollOperationBase : public SocketPollSocketOperationBase {
public:
    //! Creates the recv base with direction NB_RECV
    /** @param sock the socket (caller must have ref'd it)
        @param to_string if true, output will be a QoreStringNode; otherwise BinaryNode
    */
    DLLEXPORT SocketRecvPollOperationBase(QoreSocketObject* sock, bool to_string);

    //! Dereferences the operation; clears non-block recv flag and deref's the socket on last ref
    DLLEXPORT virtual void deref(ExceptionSink* xsink);

    //! Aborts: clears data buffer then delegates to base abort
    DLLEXPORT virtual void abort(ExceptionSink* xsink) override;

    DLLEXPORT virtual bool goalReached() const override;

    DLLEXPORT virtual const char* getStateImpl() const override;

    //! Returns the received data (ref'd), or nothing if not yet received
    DLLEXPORT virtual QoreValue getOutput() const override;

    DLLEXPORT virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override;

protected:
    SimpleRefHolder<SimpleValueQoreNode> data;
    bool to_string;
    bool received = false;

    //! Initializes the non-blocking receive state
    DLLEXPORT int initIntern(ExceptionSink* xsink);

    //! Subclasses must indicate whether abort should close the socket
    DLLEXPORT virtual bool abortNeedsClose() const override = 0;
};

//! Receives exactly N bytes non-blockingly
/** @since %Qore 2.3
*/
class SocketRecvPollOperation : public SocketRecvPollOperationBase {
public:
    //! Creates the operation to receive exactly @a size bytes
    /** @param xsink exception sink
        @param size number of bytes to receive
        @param sock the socket (will be ref'd)
        @param to_string if true, return a string; otherwise binary
    */
    DLLEXPORT SocketRecvPollOperation(ExceptionSink* xsink, ssize_t size, QoreSocketObject* sock, bool to_string);

private:
    size_t size;

    DLLEXPORT virtual bool abortNeedsClose() const override;
};

//! Receives all available data non-blockingly
/** @since %Qore 2.3
*/
class SocketRecvDataPollOperation : public SocketRecvPollOperationBase {
public:
    //! Creates the operation to receive available data
    /** @param xsink exception sink
        @param sock the socket (will be ref'd)
        @param to_string if true, return a string; otherwise binary
    */
    DLLEXPORT SocketRecvDataPollOperation(ExceptionSink* xsink, QoreSocketObject* sock, bool to_string);

    DLLEXPORT virtual bool abortNeedsClose() const override;
};

//! Receives data until a byte pattern is matched
/** @since %Qore 2.3
*/
class SocketRecvUntilBytesPollOperation : public SocketRecvPollOperationBase {
public:
    //! Creates the operation to receive until the given byte pattern
    /** @param xsink exception sink
        @param pattern the byte pattern to match (ref'd internally)
        @param sock the socket (will be ref'd)
        @param to_string if true, return a string; otherwise binary
    */
    DLLEXPORT SocketRecvUntilBytesPollOperation(ExceptionSink* xsink, const QoreStringNode* pattern,
            QoreSocketObject* sock, bool to_string);

private:
    SimpleRefHolder<QoreStringNode> pattern;

    DLLEXPORT virtual bool abortNeedsClose() const override;
};

//! Non-blocking client-side TLS handshake upgrade
/** Upgrades an already-connected plaintext socket to TLS.

    @since %Qore 2.3
*/
class SocketUpgradeClientSslPollOperation : public SocketPollSocketOperationBase {
public:
    //! Creates the TLS upgrade operation
    /** @param xsink exception sink
        @param sock the socket (will be ref'd)
    */
    DLLEXPORT SocketUpgradeClientSslPollOperation(ExceptionSink* xsink, QoreSocketObject* sock);

    //! Dereferences the operation; clears non-block and deref's the socket on last ref
    DLLEXPORT void deref(ExceptionSink* xsink);

    DLLEXPORT virtual bool goalReached() const override;

    DLLEXPORT virtual const char* getStateImpl() const override;

    DLLEXPORT virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override;

private:
    bool done = false;
};

//! Non-blocking HTTP header reader
/** Reads HTTP response or request headers from a connected socket.
    Output is a hash with header information.

    @since %Qore 2.3
*/
class SocketReadHttpHeaderPollOperation : public SocketRecvPollOperationBase {
public:
    //! Creates the HTTP header read operation
    /** @param xsink exception sink
        @param sock the socket (will be ref'd)
    */
    DLLEXPORT SocketReadHttpHeaderPollOperation(ExceptionSink* xsink, QoreSocketObject* sock);

    DLLEXPORT virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override;

    //! Returns the parsed HTTP header hash
    DLLEXPORT virtual QoreValue getOutput() const override;

    //! Aborts: clears output buffer then delegates to base abort
    DLLEXPORT virtual void abort(ExceptionSink* xsink) override;

private:
    mutable ReferenceHolder<QoreHashNode> out;

    DLLEXPORT virtual bool abortNeedsClose() const override;
};

//! Non-blocking HTTP body reader — auto-selects Content-Length, chunked TE, or connection-close
/** This operation reads the body of an HTTP response based on transfer parameters
    extracted from the response headers. It encapsulates the entire body-reading
    state machine (Content-Length fixed reads, chunked transfer encoding with
    trailer support, and connection-close EOF reads) as a single reusable
    inner poll operation.

    @par Usage
    After SocketReadHttpHeaderPollOperation completes, extract the transfer
    parameters from the headers and create this operation:
    @code{.cpp}
    auto* body_op = new SocketReadHttpBodyPollOperation(xsink, sock,
        status_code, content_length, chunked, connection_close, is_head);
    @endcode

    @since %Qore 2.3
*/
class SocketReadHttpBodyPollOperation : public SocketPollOperationBase {
public:
    //! Body reading sub-states
    enum class BodyState {
        RECV_LENGTH,        //!< Reading fixed-length body (Content-Length)
        RECV_CHUNK_SIZE,    //!< Reading chunk size line
        RECV_CHUNK_DATA,    //!< Reading chunk payload + CRLF
        RECV_CHUNK_CRLF,    //!< Reading trailer lines after last chunk
        RECV_CLOSE,         //!< Reading until EOF (Connection: close)
        DONE                //!< Body reading complete
    };

    //! Creates the body reader
    /** @param xsink exception sink
        @param sock the socket (will be ref'd)
        @param status_code HTTP response status code (for no-body detection)
        @param content_length Content-Length value (-1 = not present)
        @param chunked true if Transfer-Encoding: chunked
        @param connection_close true if Connection: close
        @param is_head true if the request method was HEAD (no body)
    */
    DLLEXPORT SocketReadHttpBodyPollOperation(ExceptionSink* xsink, QoreSocketObject* sock,
            int status_code, int64_t content_length, bool chunked,
            bool connection_close, bool is_head);

    DLLEXPORT virtual ~SocketReadHttpBodyPollOperation();

    //! Dereferences the operation; cleans up socket ref and inner ops on last ref
    DLLEXPORT void deref(ExceptionSink* xsink);

    DLLEXPORT virtual bool goalReached() const override;

    DLLEXPORT virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override;

    DLLEXPORT virtual void abort(ExceptionSink* xsink) override;

    //! Returns the accumulated body data
    DLLEXPORT virtual QoreValue getOutput() const override;

protected:
    DLLEXPORT virtual const char* getStateImpl() const override;

private:
    QoreSocketObject* sock_obj;                     //!< ref'd socket
    SocketPollOperationBase* current_op = nullptr;   //!< ref'd inner op
    BinaryNode* body = nullptr;                      //!< accumulated body (ref'd)
    BodyState body_state;
    int64_t remaining = 0;                           //!< bytes remaining for Content-Length

    DLLEXPORT void cleanup(ExceptionSink* xsink);
    DLLEXPORT void releaseCurrentOp(ExceptionSink* xsink);

    // Sub-state handlers
    DLLEXPORT QoreHashNode* handleRecvLength(ExceptionSink* xsink);
    DLLEXPORT QoreHashNode* handleRecvChunkSize(ExceptionSink* xsink);
    DLLEXPORT QoreHashNode* handleRecvChunkData(ExceptionSink* xsink);
    DLLEXPORT QoreHashNode* handleRecvChunkCrlf(ExceptionSink* xsink);
    DLLEXPORT QoreHashNode* handleRecvClose(ExceptionSink* xsink);

    //! Create the initial inner op based on body mode
    DLLEXPORT void startBodyRead(ExceptionSink* xsink);
};

#endif // _QORE_SOCKETPOLLOPERATION_H

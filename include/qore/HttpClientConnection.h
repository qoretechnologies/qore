/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    HttpClientConnection.h

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

#ifndef _QORE_HTTPCLIENTCONNECTION_H

#define _QORE_HTTPCLIENTCONNECTION_H

#include <qore/AbstractHttpPollConnection.h>

#include <string>

// Forward declarations
class QoreHashNode;
class QoreObject;
class ExceptionSink;

//! HTTP client protocol version
/** @since %Qore 2.3
*/
enum class HttpClientProtocol {
    H1,  //!< HTTP/1.1
    H2,  //!< HTTP/2
    H3   //!< HTTP/3 (QUIC)
};

//! Abstract base for C++ HTTP client connections
/** This class wraps a protocol-specific poll operation (H1, H2, or H3) and
    a socket, submits the poll op to the global AsyncIoController, and exposes
    a small C++ API for synchronous submit-and-await semantics on top.

    This is the first phase (Phase P2) of porting the basics of
    @c HttpClientConnectionManager from Qore to C++.  Later phases add the
    pool, proxy handling, and protocol cache around this class and migrate
    @c QoreHttpClientObject (sync HTTPClient) to delegate here.

    The class inherits from @ref AbstractHttpPollConnectionPriv, which
    provides the connection state machine (CONNECTING → READY → CLOSED)
    used by the protocol-specific poll operation to signal readiness.

    @par Ownership
    The connection is ref-counted (via @ref AbstractPrivateData).  Callers
    typically hold a single strong ref and call @ref closeConnection before
    dropping the ref.  Dropping the last ref without calling
    @ref closeConnection triggers an implicit close in the destructor.

    @since %Qore 2.3
*/
class HttpClientConnectionBase : public AbstractHttpPollConnectionPriv {
public:
    //! Returns the protocol (H1, H2, or H3)
    DLLEXPORT virtual HttpClientProtocol getProtocol() const = 0;

    //! Returns the number of active streams on this connection
    DLLEXPORT virtual int getActiveStreamCount() const = 0;

    //! Submit a request; returns a hash with @c stream_id (int) and
    //! @c future (Future) keys.
    /** The returned Future resolves with the response hash when the I/O
        thread completes the request, or rejects with a protocol error
        if the request fails.  The protocol-specific @c PromiseAction holds
        its own ref on the underlying @c QorePromise so the Promise stays
        alive until the action runs — callers therefore only need to keep
        the Future ref.

        @param method HTTP method (e.g. "GET", "POST")
        @param path request path (e.g. "/api/users")
        @param headers optional request headers (may be nullptr)
        @param body optional request body (may be nullptr if @a body_len is 0)
        @param body_len request body length in bytes
        @param xsink exception sink

        @return a newly-allocated hash with the keys described above
            (caller owns); @c nullptr on error (@a xsink is set)

        @throw HTTPCLIENT-STATE-ERROR if the connection is not READY
            (call @ref waitForReadyOrError first) or has been closed
        @throw HTTP1-ERROR (or protocol equivalent) if the request body
            is of an unsupported type
    */
    DLLEXPORT virtual QoreHashNode* submitRequest(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        ExceptionSink* xsink) = 0;

    //! Close the connection and release controller resources
    /** After this call, @ref isClosed returns true and no further requests
        can be submitted.  Any in-flight request is rejected with
        @c HTTP1-ABORT (or protocol equivalent).
    */
    DLLEXPORT virtual void closeConnection(ExceptionSink* xsink) = 0;

    //! Blocks until the connection transitions out of CONNECTING
    /** @param timeout_ms wait budget in milliseconds (0 or negative = wait forever)
        @param xsink exception sink — set with a connection error if the
            connection transitioned to CLOSED with an error during the wait

        @return @c true if the connection is READY; @c false if the wait
            timed out (no exception raised) or the connection is CLOSED
            (exception raised via @a xsink)
    */
    DLLEXPORT bool waitForReadyOrError(int64_t timeout_ms, ExceptionSink* xsink);

    //! Target hostname used for Host header and connect
    DLLEXPORT const char* getTargetHost() const {
        return target_host.c_str();
    }

    //! Target port
    DLLEXPORT int getTargetPort() const {
        return target_port;
    }

    //! True if the connection requires SSL/TLS
    DLLEXPORT bool isSslRequired() const {
        return ssl_required;
    }

protected:
    DLLLOCAL HttpClientConnectionBase(std::string target_host, int target_port,
            bool ssl_required)
        : target_host(std::move(target_host)),
          target_port(target_port),
          ssl_required(ssl_required) {
    }

    //! Returns the referenced protocol-specific error hash (if any)
    /** Called by @ref waitForReadyOrError when the state transitioned to
        CLOSED during the wait.  The returned hash has @c err (string) and
        @c desc (string) keys; caller owns the returned ref (or nullptr if
        no error info is available).
    */
    DLLLOCAL virtual QoreHashNode* getReferencedErrorInfo() = 0;

    std::string target_host;
    int target_port;
    bool ssl_required;
};

#endif // _QORE_HTTPCLIENTCONNECTION_H

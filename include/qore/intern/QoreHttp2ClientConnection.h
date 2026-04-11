/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHttp2ClientConnection.h

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

#ifndef _QORE_INTERN_QOREHTTP2CLIENTCONNECTION_H

#define _QORE_INTERN_QOREHTTP2CLIENTCONNECTION_H

#include <qore/HttpClientConnection.h>

#include <string>

class QoreSocketObject;
class Http2ClientPollOperationPriv;

//! HTTP/2 C++ client connection.
/** Wraps a @ref Http2ClientPollOperationPriv and the socket it operates on,
    submits the poll op to the global AsyncIoController on construction, and
    exposes @ref submitRequest for sync-over-async multiplexed request
    dispatch.

    Phase P4 of the HttpClientConnectionManager C++ port (see
    @c design/http-client-manager-cpp-port.md).  Inherits the close-hook
    plumbing (manager_, setManager, onClosedHook) from
    @ref HttpClientConnectionBase — no per-protocol overrides needed.

    @par Multiplexing
    HTTP/2 supports multiple concurrent streams per connection.  The
    @ref getMaxConcurrentStreams override returns the configured cap (or
    the negotiated server SETTINGS value once h2_confirmed is set).

    @par H2c probe
    For plain-HTTP h2c (HTTP/2 prior knowledge), the server must respond
    with a valid SETTINGS frame within the connect timeout to confirm h2
    support.  If the server doesn't speak h2, the connection transitions
    to CLOSED with an error and the manager evicts.  Phase P3 manager
    does NOT yet implement the h2-to-h1 fallback that the existing Qore
    @c HttpClientConnectionManager has — that lives in Phase P6 / the
    Qore subclass.

    @since %Qore 2.3
*/
class Http2ClientConnection : public HttpClientConnectionBase {
public:
    //! Creates a new HTTP/2 client connection.
    /** Creates a socket (with ALPN @c "h2" set if SSL), a
        @ref SocketConnectPollOperation, and a
        @ref Http2ClientPollOperationPriv wrapping the two, submits the
        poll op to the global AsyncIoController, and returns.  The
        connection is in CONNECTING state until @ref waitForReadyOrError
        returns.

        @param target_host target hostname (used for the @c :authority
            pseudo-header and connect)
        @param target_port target TCP port
        @param ssl_required if @c true, SSL/TLS is required (HTTPS) and
            ALPN @c "h2" is configured on the socket BEFORE connecting
        @param max_concurrent_streams advisory cap on concurrent streams
            per connection (0 = unlimited; the actual cap is also bounded
            by the server's @c MAX_CONCURRENT_STREAMS setting)
        @param xsink exception sink — set on construction failure
    */
    DLLLOCAL Http2ClientConnection(const char* target_host, int target_port,
        bool ssl_required, int max_concurrent_streams, ExceptionSink* xsink);

    DLLLOCAL virtual ~Http2ClientConnection();

    //! Sets the controller-submission owner string before submission.
    /** See @ref Http1ClientConnection::setOwner for the convention.
        Phase P4 has the same Phase-P2-style limitation: the constructor
        submits eagerly, so calling @c setOwner after construction has
        no effect on the already-submitted op.  The setter is in place
        for forward compatibility with a two-phase construction model.
    */
    DLLLOCAL void setOwner(const char* owner) {
        if (owner) {
            owner_str = owner;
        }
    }

    // --- HttpClientConnectionBase overrides ---

    HttpClientProtocol getProtocol() const override {
        return HttpClientProtocol::H2;
    }

    int getMaxConcurrentStreams() const override {
        return max_concurrent_streams_;
    }

    DLLEXPORT int getActiveStreamCount() const override;

    DLLEXPORT QoreHashNode* submitRequest(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        ExceptionSink* xsink) override;

    DLLEXPORT void closeConnection(ExceptionSink* xsink) override;

protected:
    DLLLOCAL QoreHashNode* getReferencedErrorInfo() override;

private:
    //! The socket QoreObject (ref'd).
    QoreObject* sock_obj = nullptr;

    //! The socket priv (raw pointer — ownership via @ref sock_obj).
    QoreSocketObject* sock_priv = nullptr;

    //! The poll op QoreObject (ref'd).
    QoreObject* poll_op_obj = nullptr;

    //! The poll op priv (raw pointer — ownership via @ref poll_op_obj).
    Http2ClientPollOperationPriv* poll_op_priv = nullptr;

    //! True once the poll op has been submitted to the controller.
    bool submitted_to_controller = false;

    //! Configured max concurrent streams (0 = unlimited).
    int max_concurrent_streams_ = 0;

    //! Owner string for controller submit info hash; default per-instance.
    std::string owner_str;

    //! Builds the C++ pieces and submits to the controller.
    DLLLOCAL int buildAndSubmit(ExceptionSink* xsink);
};

#endif // _QORE_INTERN_QOREHTTP2CLIENTCONNECTION_H

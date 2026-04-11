/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHttp1ClientConnection.h

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

#ifndef _QORE_INTERN_QOREHTTP1CLIENTCONNECTION_H

#define _QORE_INTERN_QOREHTTP1CLIENTCONNECTION_H

#include <qore/HttpClientConnection.h>

#include <string>

class QoreSocketObject;
class Http1ClientPollOperationPriv;

//! HTTP/1.1 C++ client connection
/** Wraps a @ref Http1ClientPollOperationPriv and the socket it operates on,
    submits the poll op to the global AsyncIoController on construction, and
    exposes @ref submitRequest for sync-over-async request dispatch.

    The connection starts in CONNECTING state; callers must call
    @ref waitForReadyOrError before @ref submitRequest.

    @since %Qore 2.3
*/
class Http1ClientConnection : public HttpClientConnectionBase {
public:
    //! Creates a new HTTP/1.1 client connection
    /** Creates a socket, a @ref SocketConnectPollOperation, and a
        @ref Http1ClientPollOperationPriv wrapping the two, submits the
        poll op to the global AsyncIoController, and returns.  The
        connection is in CONNECTING state until @ref waitForReadyOrError
        returns.

        @param target_host target hostname (used for the Host header)
        @param target_port target TCP port
        @param ssl_required if @c true, SSL/TLS is required (HTTPS)
        @param xsink exception sink — set on construction failure
    */
    DLLLOCAL Http1ClientConnection(const char* target_host, int target_port,
        bool ssl_required, ExceptionSink* xsink);

    DLLLOCAL virtual ~Http1ClientConnection();

    //! Sets the controller-submission owner string before submission to
    //! the AsyncIoController.
    /** Must be called BEFORE construction submits the poll op (i.e., the
        manager constructs an "owner-pending" connection, calls @ref setOwner,
        then triggers submission).  In the simple unconstructed-by-manager
        case, the constructor uses a default per-instance owner string.

        For now (Phase P2), the constructor submits immediately and there is
        no opportunity to call setOwner before submission.  Phase P3 will
        introduce a two-phase construction model where the manager creates
        the connection in an unsubmitted state, calls setOwner, then submits.
        Until then this setter just stores the value for the next submission.

        @param owner the owner string to use in the SocketPollOperationInfo
            hash; must outlive this connection (or be copied internally —
            this implementation copies)

        @since %Qore 2.3
    */
    DLLLOCAL void setOwner(const char* owner) {
        if (owner) {
            owner_str = owner;
        }
    }

    // --- HttpClientConnectionBase overrides ---

    HttpClientProtocol getProtocol() const override {
        return HttpClientProtocol::H1;
    }

    DLLEXPORT int getActiveStreamCount() const override;

    DLLEXPORT QoreHashNode* submitRequest(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        ExceptionSink* xsink) override;

    DLLEXPORT void closeConnection(ExceptionSink* xsink) override;

protected:
    DLLLOCAL QoreHashNode* getReferencedErrorInfo() override;

private:
    //! The socket QoreObject (ref'd).  Owns a QoreSocketObject priv.
    QoreObject* sock_obj = nullptr;

    //! The socket priv (raw pointer — ownership via sock_obj)
    QoreSocketObject* sock_priv = nullptr;

    //! The poll op QoreObject (ref'd).  Owns a Http1ClientPollOperationPriv.
    QoreObject* poll_op_obj = nullptr;

    //! The poll op priv (raw pointer — ownership via poll_op_obj).
    Http1ClientPollOperationPriv* poll_op_priv = nullptr;

    //! True once the poll op has been submitted to the controller
    bool submitted_to_controller = false;

    //! Owner string used for the controller submit info hash.
    /** Defaults to a per-instance string built from @c this in
        @ref buildAndSubmit.  Phase P3+ managers can override via
        @ref setOwner before construction-time submission to use their own
        owner string and benefit from @c cancelByOwner cleanup.

        @note @c manager_ and @c onclose_lock live on
        @ref HttpClientConnectionBase (the base class) so all H1/H2/H3
        connection wrappers automatically participate in the close-hook
        protocol without per-protocol overrides.
    */
    std::string owner_str;

    //! Builds the C++ pieces and submits to the controller.  Called from
    //! the constructor.
    DLLLOCAL int buildAndSubmit(ExceptionSink* xsink);
};

#endif // _QORE_INTERN_QOREHTTP1CLIENTCONNECTION_H

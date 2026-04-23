/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    NegotiatingConnectionPollOp.h

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

#ifndef _QORE_INTERN_NEGOTIATINGCONNECTIONPOLLOP_H

#define _QORE_INTERN_NEGOTIATINGCONNECTIONPOLLOP_H

#include <qore/HttpClientConnection.h>
#include <qore/ReferenceHolder.h>
#include <qore/SocketPollOperationBase.h>
#include "qore/intern/QoreHttp1ClientConnection.h"

#include <atomic>
#include <string>

class QoreSocketObject;
class SocketPollOperationBase;
class NegotiatingHttpClientConnection;

//! C++ poll op that drives TCP connect + TLS handshake + ALPN read.
/** Runs as an async poll op on the global AsyncIoController.  Inner
    @c current_op is a @ref SocketConnectPollOperation with @c ssl=true
    that performs the TCP connect followed by the TLS handshake with an
    ALPN offer of @c {"h2","http/1.1"}.

    State machine:
    - @c CONNECTING: drive the inner connect op; on goalReached read the
      negotiated ALPN from the socket, store it on the owning
      @ref NegotiatingHttpClientConnection, transition to @c DECIDED, and
      fire @ref AbstractHttpPollConnectionPriv::onConnectionReady.
    - @c DECIDED: the app thread has everything it needs to hand off the
      socket to a concrete H1 or H2 connection.  @ref continuePoll returns
      @c nullptr here, which signals the controller to finalize the op,
      unregister the socket from the event loop, and call
      @ref needsCloseOnComplete.  We return @c false there so the socket
      is NOT auto-closed — the caller will wrap it in an
      @ref Http1ClientConnection or @ref Http2ClientConnection via the
      Phase 2 adopt-socket constructor.
    - @c CLOSED: terminal; set on error paths or explicit abort.

    The inner @ref SocketConnectPollOperation clears the socket's
    @c non_block_flags when its refcount drops to 0, so by the time the
    controller has finished finalizing us, the socket is ready to be
    re-submitted under the adopt-socket path.

    Not thread-safe by design: @ref continuePoll runs only on the I/O
    thread, the app thread reads the result after
    @ref AbstractHttpPollConnectionPriv::waitForReady returns.  The
    hand-off between threads goes through atomic state + the connection
    priv's condition variable.

    @since %Qore 2.3
*/
class NegotiatingConnectionPollOpPriv : public SocketPollOperationBase {
public:
    //! State machine values.
    enum class NegState {
        CONNECTING,
        DECIDED,
        CLOSED
    };

    //! Creates the poll op.
    /** @param self the QoreObject wrapping this private data (may be
            nullptr; set later via @ref setSelf)
        @param sock the socket (ref'd by caller, ownership transferred)
        @param connect_op the @ref SocketConnectPollOperation driving
            TCP connect + SSL handshake (ref'd by caller, ownership
            transferred)
        @param target_host target hostname (for error reporting)
        @param target_port target port
        @param owner_conn the owning @ref NegotiatingHttpClientConnection
            (raw pointer — the Qore @c "connection_ref" member on the
            poll op QoreObject holds the DGC-visible ref)
    */
    DLLLOCAL NegotiatingConnectionPollOpPriv(QoreObject* self,
        QoreSocketObject* sock, SocketPollOperationBase* connect_op,
        std::string target_host, int target_port,
        NegotiatingHttpClientConnection* owner_conn);

    DLLLOCAL virtual ~NegotiatingConnectionPollOpPriv();

    // --- SocketPollOperationBase overrides ---

    DLLLOCAL bool goalReached() const override {
        NegState s = neg_state.load(std::memory_order_acquire);
        return s == NegState::DECIDED || s == NegState::CLOSED;
    }

    DLLLOCAL bool needsCloseOnComplete() const override {
        // Only the CLOSED (error) path closes the socket.  DECIDED leaves
        // the socket alive for the caller to hand off to an adopt-socket
        // constructor.
        return neg_state.load(std::memory_order_acquire) == NegState::CLOSED;
    }

    DLLLOCAL QoreHashNode* continuePoll(ExceptionSink* xsink) override;
    DLLLOCAL void abort(ExceptionSink* xsink) override;
    DLLLOCAL QoreValue getOutput() const override { return QoreValue(); }

    // --- Accessors (app thread) ---

    DLLLOCAL NegState getNegState() const {
        return neg_state.load(std::memory_order_acquire);
    }

    DLLLOCAL QoreHashNode* getReferencedErrorInfo() const {
        if (error_info) {
            error_info->ref();
        }
        return error_info;
    }

    //! Called by the destructor of the owning connection to break the
    //! raw back-pointer before the owner is freed.  Safe on any thread
    //! because the owner has already disarmed us.
    DLLLOCAL void clearOwner() {
        owner_conn = nullptr;
    }

protected:
    DLLLOCAL const char* getStateImpl() const override;

private:
    std::atomic<NegState> neg_state{NegState::CONNECTING};

    //! The inner connect+SSL poll op (ref'd, cleared after completion).
    SocketPollOperationBase* current_op;

    //! The socket priv (ref'd; ownership stays with us until cleanup).
    QoreSocketObject* sock_obj;

    //! Target hostname (for diagnostics only).
    std::string target_host;

    //! Target port (for diagnostics only).
    int target_port;

    //! Raw back-pointer to the owning connection — provides access to
    //! the ALPN storage slot and the @c onConnectionReady hook.  The
    //! strong Qore ref (for DGC cycle detection) is held via the poll
    //! op QoreObject's @c "connection_ref" member.
    NegotiatingHttpClientConnection* owner_conn = nullptr;

    //! Error info hash (ref'd) — set on failure paths.
    QoreHashNode* error_info = nullptr;

    DLLLOCAL QoreHashNode* handleConnecting(ExceptionSink* xsink);
    DLLLOCAL void setError(const char* err, const char* desc, ExceptionSink* xsink);
    DLLLOCAL void releaseCurrentOp(ExceptionSink* xsink);
};

//! Internal helper connection that drives ALPN negotiation to completion
//! before being replaced by an @ref Http1ClientConnection or
//! @ref Http2ClientConnection via the Phase 2 adopt-socket constructors.
/** Inherits @ref HttpClientConnectionBase so it can use the base
    @c CONNECTING -> @c READY state machine (and @ref waitForReadyOrError)
    without duplicating the condition-variable plumbing.

    This class is NEVER returned from
    @ref HttpClientConnectionManagerBase::createConnection — callers use
    it as a waiter during the negotiation phase only.  After
    @ref waitForReadyOrError returns true, the caller invokes
    @ref takeOver to:

    1. Read the negotiated ALPN from the poll op.
    2. Release the socket from our internal ownership (we @c deref
       @c sock_obj, the inner @ref SocketConnectPollOperation's dtor has
       already cleared @c non_block_flags when its refcount dropped).
    3. Construct the concrete @ref Http1ClientConnection or
       @ref Http2ClientConnection via the Phase 2 adopt-socket ctor,
       passing ownership of the socket priv.
    4. Return the concrete connection.

    @since %Qore 2.3
*/
class NegotiatingHttpClientConnection : public HttpClientConnectionBase {
public:
    //! Starts the negotiation: creates a socket with ALPN
    //! @c {"h2","http/1.1"}, applies the supplied SSL config, creates a
    //! @ref SocketConnectPollOperation with @c ssl=true, and submits to
    //! the AsyncIoController.
    /** @param target_host target hostname
        @param target_port target port
        @param ssl_config SSL verification and client-cert config applied
            to the socket before the handshake starts
        @param xsink exception sink — set on construction failure
    */
    DLLLOCAL NegotiatingHttpClientConnection(const char* target_host,
        int target_port, const Http1SslConfig& ssl_config,
        ExceptionSink* xsink);

    DLLLOCAL virtual ~NegotiatingHttpClientConnection();

    //! Performs the handover to a concrete H1/H2 connection.
    /** Must be called only after @ref waitForReadyOrError returned true.
        Transfers ownership of the adopted socket to the returned
        connection and leaves this object in a defunct state (callers
        must deref it immediately after).

        @param max_concurrent_streams H2 stream cap (ignored for H1)
        @param mgr owning manager — passed through to the concrete
            connection's adopt-socket constructor
        @param xsink exception sink

        @return a newly-constructed @ref Http1ClientConnection or
            @ref Http2ClientConnection (caller owns one ref), or
            @c nullptr on failure
    */
    DLLLOCAL HttpClientConnectionBase* takeOver(int max_concurrent_streams,
        HttpClientConnectionManagerBase* mgr, ExceptionSink* xsink);

    DLLLOCAL void closeConnection(ExceptionSink* xsink) override;

    //! Called from @ref NegotiatingConnectionPollOpPriv on the I/O thread
    //! once ALPN has been decided.  Stores the negotiated protocol id
    //! and transitions the base class state to @c READY.  Safe to call
    //! only once per object lifetime.
    DLLLOCAL void onAlpnDecided(std::string alpn);

    //! Returns the negotiated ALPN id (valid after @ref waitForReadyOrError).
    DLLLOCAL const std::string& getNegotiatedAlpn() const {
        return alpn_result;
    }

protected:
    DLLLOCAL QoreHashNode* getReferencedErrorInfo() override;

private:
    QoreObject* sock_obj = nullptr;         //!< ref'd
    QoreSocketObject* sock_priv = nullptr;  //!< raw — owned by @ref sock_obj
    QoreObject* poll_op_obj = nullptr;      //!< ref'd
    NegotiatingConnectionPollOpPriv* neg_priv = nullptr;  //!< raw — owned by @ref poll_op_obj
    bool submitted_to_controller = false;
    bool taken_over = false;

    //! Negotiated ALPN id, populated by @ref onAlpnDecided (I/O thread)
    //! and read by @ref takeOver (app thread).  The store happens
    //! before @c onConnectionReady, the load happens after
    //! @c waitForReadyOrError, so the transition through the
    //! @ref AbstractHttpPollConnectionPriv condition variable provides
    //! the necessary memory ordering.
    std::string alpn_result;

    DLLLOCAL int buildAndSubmit(const Http1SslConfig& ssl_config, ExceptionSink* xsink);
};

#endif // _QORE_INTERN_NEGOTIATINGCONNECTIONPOLLOP_H

/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_HttpAcceptPollOperationBase.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_CLASS_HTTPACCEPTPOLLOPERATIONBASE_H

#define _QORE_CLASS_HTTPACCEPTPOLLOPERATIONBASE_H

#include "qore/SocketPollOperationBase.h"
#include "qore/QoreSocketObject.h"
#include "qore/QoreSSLCertificate.h"
#include "qore/QoreSSLPrivateKey.h"
#include "qore/ReferenceHolder.h"

//! C++ private data for HttpAcceptPollOperationBase
/** Composite poll operation for TCP accept + optional SSL handshake.

    This operation has two states:
    1. **ACCEPTING**: Delegates to SocketAcceptPollOperation — waits for an incoming TCP connection.
       When accept completes: gets client socket, sets TCP_NODELAY, then either transitions to
       SSL_HANDSHAKE or goes directly to COMPLETE depending on SSL config.
    2. **SSL_HANDSHAKE**: Delegates to SocketUpgradeServerSslPollOperation — performs the TLS
       handshake on the accepted client socket. When complete: checks ALPN protocol negotiation,
       validates HTTP/2 requirement, then transitions to COMPLETE.

    The SSL certificate, private key, and ALPN configuration are applied to the client socket
    when transitioning from ACCEPTING to SSL_HANDSHAKE.

    The Qore wrapper class (HttpAcceptPollOperation) adds completion context and onComplete()
    callback dispatch.

    @since %Qore 2.3
*/
class HttpAcceptPollOperationPriv : public SocketPollOperationBase {
public:
    //! Accept state machine states
    enum class AcceptState {
        ACCEPTING,      //!< Waiting for incoming TCP connection
        SSL_HANDSHAKE,  //!< Performing SSL/TLS handshake on accepted socket
        COMPLETE        //!< Operation finished
    };

    //! Creates the accept poll operation
    /** @param self the QoreObject wrapping this private data
        @param accept_op the inner SocketAcceptPollOperation from listener.startPollAccept()
                         (already ref'd, ownership transferred)
        @param accept_op_obj the QoreObject wrapping the accept op (ref'd for DGC)
        @param listener the listener socket (ref'd for lifetime)
        @param ssl true if SSL is required for this listener
        @param cert SSL certificate object (nullable; QoreSSLCertificate*, ref'd, ownership transferred)
        @param key SSL private key object (nullable; QoreSSLPrivateKey*, ref'd, ownership transferred)
        @param ssl_verify_flags SSL verification flags (e.g. SSL_VERIFY_PEER)
        @param ssl_accept_all_certs if true, accept all client certificates including self-signed
        @param http_version HTTP version mode: "auto", "1.0", "1.1", "2.0", "2", or "h2c"
        @param do_ssl_handshake if false, defer SSL handshake to the caller
    */
    DLLLOCAL HttpAcceptPollOperationPriv(QoreObject* self,
        SocketPollOperationBase* accept_op,
        QoreObject* accept_op_obj,
        QoreSocketObject* listener,
        bool ssl,
        QoreSSLCertificate* cert,
        QoreSSLPrivateKey* key,
        int ssl_verify_flags,
        bool ssl_accept_all_certs,
        const char* http_version,
        bool do_ssl_handshake);

    DLLLOCAL ~HttpAcceptPollOperationPriv();

    //! Returns true when the operation is complete (accept + optional SSL done)
    DLLLOCAL bool goalReached() const override;

    //! Drives the state machine: delegates to inner ops, transitions on completion
    DLLLOCAL QoreHashNode* continuePoll(ExceptionSink* xsink) override;

    //! Aborts the operation: aborts inner op, closes client socket if open
    DLLLOCAL void abort(ExceptionSink* xsink) override;

    //! Returns the operation output hash
    /** The returned hash contains:
        - \c sock: the accepted QoreSocketObject (ref'd)
        - \c ssl: true if SSL
        - \c is_http2: true if HTTP/2 was negotiated
        - \c negotiated_protocol: ALPN protocol string or nullptr
        - \c http_version: the configured HTTP version mode
    */
    DLLLOCAL QoreValue getOutput() const override;

    //! Returns true if HTTP/2 was negotiated via ALPN or h2c
    DLLLOCAL bool isHttp2() const {
        return is_http2;
    }

    //! Returns true if SSL is configured
    DLLLOCAL bool isSsl() const {
        return ssl;
    }

    //! Returns the negotiated ALPN protocol, or nullptr
    DLLLOCAL const char* getNegotiatedProtocol() const {
        return negotiated_protocol.empty() ? nullptr : negotiated_protocol.c_str();
    }

    //! Returns the client socket QoreObject (ref'd), or nullptr if not yet accepted
    DLLLOCAL QoreObject* getReferencedClientSocketObj() const {
        if (client_sock_obj) {
            client_sock_obj->ref();
        }
        return client_sock_obj;
    }

    //! Returns the current state as a string
    DLLLOCAL const char* getAcceptState() const {
        switch (accept_state) {
            case AcceptState::ACCEPTING:     return "accepting";
            case AcceptState::SSL_HANDSHAKE: return "ssl_handshake";
            case AcceptState::COMPLETE:      return "complete";
            default:                         return "unknown";
        }
    }

    //! Releases all internal references
    DLLLOCAL void cleanup(ExceptionSink* xsink);

protected:
    DLLLOCAL const char* getStateImpl() const override;

private:
    QoreSocketObject* listener_sock;        //!< ref'd listener socket
    SocketPollOperationBase* current_op;     //!< ref'd inner poll operation (accept or ssl)
    QoreObject* current_op_obj;             //!< ref'd QoreObject wrapping current_op (for DGC)
    QoreSocketObject* client_sock;          //!< ref'd client socket (after accept completes)
    QoreObject* client_sock_obj;            //!< ref'd QoreObject wrapping client_sock (prevents close)
    AcceptState accept_state;

    // Configuration
    bool ssl;                               //!< SSL required
    bool do_ssl_handshake;                  //!< Perform SSL handshake in this op
    bool is_http2;                          //!< HTTP/2 was negotiated
    int ssl_verify_flags;                   //!< SSL verification flags
    bool ssl_accept_all_certs;              //!< Accept all client certificates
    std::string http_version;               //!< HTTP version mode string
    std::string negotiated_protocol;        //!< Negotiated ALPN protocol

    // SSL config (stored for deferred application to client socket)
    QoreSSLCertificate* cert;               //!< ref'd SSL certificate (nullable)
    QoreSSLPrivateKey* key;                 //!< ref'd SSL private key (nullable)

    //! Handles the accept-complete transition: gets client socket, checks SSL state
    /** @return poll info hash if more polling needed, nullptr if done or transitioning
    */
    DLLLOCAL QoreHashNode* handleAcceptComplete(ExceptionSink* xsink);

    //! Handles the SSL handshake completion: checks ALPN, validates H2 requirement
    DLLLOCAL QoreHashNode* handleSslComplete(ExceptionSink* xsink);

    //! Configures SSL on the client socket and starts the SSL handshake operation
    /** @return 0 on success, -1 on error (exception raised)
    */
    DLLLOCAL int startSslHandshake(ExceptionSink* xsink);

    //! Checks ALPN negotiation and validates HTTP/2 requirement
    /** @return 0 on success, -1 if H2 required but not negotiated (exception raised)
    */
    DLLLOCAL int checkAlpn(ExceptionSink* xsink);

    //! Builds the ALPN protocol list based on http_version config
    DLLLOCAL QoreListNode* buildAlpnList() const;

    //! Releases the current inner operation and its wrapper object
    DLLLOCAL void releaseCurrentOp(ExceptionSink* xsink);
};

DLLLOCAL QoreClass* initHttpAcceptPollOperationBaseClass(QoreNamespace& qorens);

#endif // _QORE_CLASS_HTTPACCEPTPOLLOPERATIONBASE_H

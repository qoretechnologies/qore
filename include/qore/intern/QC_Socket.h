/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_Socket.h

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

#ifndef _QORE_CLASS_SOCKET_H

#define _QORE_CLASS_SOCKET_H

DLLLOCAL QoreClass* initSocketClass(QoreNamespace& qorens);
DLLEXPORT extern qore_classid_t CID_SOCKET;
DLLEXPORT extern QoreClass* QC_SOCKET;

DLLLOCAL TypedHashDecl* init_hashdecl_ExtraPollFdInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_SocketPollInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_SseMessageInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_DatagramInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_QuicGoawayStateInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_LingeringCloseInfo(QoreNamespace& ns);

//! Raises the exception for a synchronous accept operation that ended without a connection and without an exception
/** The I/O controller cancels the pending operations of a socket that is closed by another thread
    (AsyncIoControllerPriv::close()), so a canceled result means that the listening socket was closed while the call
    was waiting for a connection: \c SOCKET-CLOSED is raised, as when the controller finds the socket closed while
    waiting for I/O readiness.  Any other result without a connection is an internal error (\c SOCKET-ACCEPT-ERROR);
    a timeout is reported with a \c SOCKET-TIMEOUT exception in the result and is not handled here.

    @param result the result of the poll operation; may be null
    @param method the name of the Socket method, for the exception description
    @param xsink for the exception raised
*/
DLLLOCAL void qore_socket_raise_accept_without_connection(const QoreHashNode* result, const char* method,
        ExceptionSink* xsink);

//! Returns true if \a en is the errno of a write that failed because the peer closed or reset the connection
/** \c ECONNRESET, \c EPIPE or \c ECONNABORTED: the connection is gone, so a read returns at once with what the peer
    sent before it closed, and then EOF or an error.  A plain socket raises such a write failure as
    \c SOCKET-SEND-ERROR, and a TLS socket as \c SOCKET-SSL-ERROR, both with the errno as the exception argument.

    @since %Qore 3.0
*/
DLLLOCAL bool qore_socket_errno_is_peer_close(int64 en);

class QoreSocketObject;

//! Waits for the HTTP/3 SETTINGS of the server of the QUIC client connection of a socket (Socket::waitForQuicSettings())
/** @return 1 if they have been received, 0 on timeout, -1 if an exception was raised
*/
DLLLOCAL int qore_socket_object_wait_for_quic_settings(QoreSocketObject* s, int64 timeout_ms, ExceptionSink* xsink);

#include <qore/QoreSocket.h>
#include <qore/ReferenceHolder.h>
#include <qore/AbstractPrivateData.h>
#include <qore/QoreThreadLock.h>
#include <qore/QoreSocketObject.h>
#include <qore/qore_thread.h>

#include <atomic>
#include "qore/intern/QC_SSLCertificate.h"
#include "qore/intern/QC_SSLPrivateKey.h"
#include "qore/intern/QoreIoClassesOps.h"

//! Raises SOCKET-SERVER-OWNED-ERROR if the calling code may not use the connection socket of a server (qore#7338)
/** @param kind 0 for a method that reads, writes, changes or polls the connection (refused to sandboxed code), 1 for
    close() and shutdown*() (refused to code that a sandbox's policy governs)

    @return 0 if the call is allowed, -1 if not (the exception is raised)
*/
DLLLOCAL int qore_socket_server_guard_check(int kind, ExceptionSink* xsink);

class my_socket_priv {
public:
    QoreSocket* socket;
    QoreSSLCertificate* cert = nullptr;
    QoreSSLPrivateKey* pk = nullptr;
    // Consumed by the first HTTP client operation constructed on this socket. Ordinary socket TLS APIs
    // use cert/pk only and cannot read or install these references.
    SimpleRefHolder<QoreSSLCertificate> http_handshake_cert;
    SimpleRefHolder<QoreSSLPrivateKey> http_handshake_key;
    mutable QoreThreadLock m;
    std::string ssl_cipher_name_cache;
    std::string ssl_cipher_version_cache;
    unsigned non_block_flags = 0;
    //! set when the server drains the connection (Socket::drainHttpConnection(), qore#5899): 1 if a keep-alive
    //! operation ends after the response that it sends instead of waiting for the next request, 2 if, in addition, an
    //! operation that waits for a request ends at once when none has arrived, even if the connection has not had one
    std::atomic<int> http_draining{0};
    int non_block_accept_count = 0;
    //! true if Socket::accept() and Socket::acceptSSL() resolve the peer's hostname for the accepted socket's
    //! \c source_host member (see Socket::setSourceHostLookup())
    std::atomic<bool> source_host_lookup{false};
    // A connection can supply TLS objects for use without exporting them in an HTTP client's configuration.
    bool opaque_cert = false;
    bool opaque_key = false;
    //! the Socket object of a server's connection whose methods sandboxed code may not call (qore#7338)
    /** Set once by Socket::setServerOwned(); cleared when that object is destroyed.  A guarded method compares its
        object with it; the alias of the connection (see server_alias), another object, is not guarded.
    */
    std::atomic<QoreObject*> server_owned{nullptr};
    //! the alias of a server's connection socket: a Socket object with the same private data, never guarded
    /** Not referenced: the server holds it; cleared when it is destroyed (qore#7338)
    */
    QoreObject* server_alias = nullptr;
    //! the reference to the connection socket that the private data holds while its alias exists (qore#7338)
    QoreObject* server_owner_ref = nullptr;
    //! true once the alias of the connection socket was made: it is made once (qore#7338)
    bool server_alias_made = false;
    //! the number of Socket objects that share the private data (the connection socket and its alias)
    int obj_count = 1;
    int async_io_count = 0;
    int async_sequence_owner_tid[3] = {-1, -1, -1};
    int async_sequence_count[3] = {0, 0, 0};
    bool valid = true;

    //! Transfers HTTP handshake credentials to one poll operation and identifies the original connection.
    DLLLOCAL int64 takeHttpHandshakeCredentials(SimpleRefHolder<QoreSSLCertificate>& certificate,
            SimpleRefHolder<QoreSSLPrivateKey>& private_key) {
        AutoLocker al(m);
        certificate = http_handshake_cert.release();
        private_key = http_handshake_key.release();
        return certificate || private_key ? socket->getConnectionId() : -1;
    }

    DLLLOCAL my_socket_priv(QoreSocket* s, QoreSSLCertificate* c = nullptr, QoreSSLPrivateKey* p = nullptr);

    DLLLOCAL my_socket_priv();

    DLLLOCAL ~my_socket_priv() {
        if (cert) {
            cert->deref();
        }
        if (pk) {
            pk->deref();
        }

        delete socket;
    }

    //! Invalidates the object
    DLLLOCAL void invalidate() {
        // must be called with the lock held
        assert(m.trylock());

        if (valid) {
            valid = false;
        }
    }

    //! Throws an exception if the object is no longer valid
    DLLLOCAL int checkValid(ExceptionSink* xsink) {
        // must be called with the lock held
        assert(m.trylock());

        if (!valid) {
            xsink->raiseException("OBJECT-ALREADY-DELETED", "the underlying socket object has already been deleted "
                "and can no longer be used");
            return -1;
        }
        return 0;
    }

    //! Returns true if the async controller or a controller-backed sequence currently owns this socket
    DLLLOCAL bool hasAsyncIoOwner() const {
        // must be called with the lock held
        assert(m.trylock());

        return async_io_count > 0 || non_block_flags || non_block_accept_count > 0;
    }

    //! Throws an exception if the socket is owned by async controller execution
    /** @return 0 if sync I/O is allowed, -1 if not (exception raised on @a xsink)
        @since %Qore 3.0
    */
    DLLLOCAL int checkSyncAllowed(ExceptionSink* xsink) {
        // must be called with the lock held
        assert(m.trylock());

        if (hasAsyncIoOwner()) {
            xsink->raiseException("SOCKET-ASYNC-MODE-ERROR",
                "cannot perform synchronous I/O on a socket managed by "
                "the async I/O controller");
            return -1;
        }
        return 0;
    }

    //! Returns a printable operation direction name for diagnostics
    DLLLOCAL static const char* getNonBlockDirectionName(unsigned direction) {
        switch (direction) {
            case NB_SEND:
                return "send";
            case NB_RECV:
                return "receive";
            case NB_CONNECT:
                return "connect";
            default:
                return "I/O";
        }
    }

    //! Returns the async sequence array index for a single non-blocking direction bit
    DLLLOCAL static int getAsyncSequenceIndex(unsigned direction) {
        switch (direction) {
            case NB_SEND:
                return 0;
            case NB_RECV:
                return 1;
            case NB_CONNECT:
                return 2;
            default:
                assert(false);
                return 0;
        }
    }

    //! Throws an exception if another thread owns a multi-step async sequence for @a direction
    DLLLOCAL int checkAsyncSequenceAllowed(ExceptionSink* xsink, unsigned direction) const {
        // must be called with the lock held
        assert(m.trylock());

        return checkAsyncSequenceAllowedForTid(xsink, direction, q_gettid());
    }

    //! Throws an exception if another thread owns a multi-step async sequence for @a direction
    DLLLOCAL int checkAsyncSequenceAllowedForTid(ExceptionSink* xsink, unsigned direction, int tid) const {
        // must be called with the lock held
        assert(m.trylock());

        unsigned flags = direction & NB_ALL;
        if ((flags & NB_SEND) && checkAsyncSequenceAllowedIntern(xsink, NB_SEND, tid)) {
            return -1;
        }
        if ((flags & NB_RECV) && checkAsyncSequenceAllowedIntern(xsink, NB_RECV, tid)) {
            return -1;
        }
        if ((flags & NB_CONNECT) && checkAsyncSequenceAllowedIntern(xsink, NB_CONNECT, tid)) {
            return -1;
        }
        return 0;
    }

    //! Throws an exception if any non-blocking operation is in progress or is not valid
    DLLLOCAL int checkNonBlock(ExceptionSink* xsink) {
        // must be called with the lock held
        assert(m.trylock());

        // Prevent sync operations on a socket managed by the async I/O controller
        if (checkSyncAllowed(xsink)) {
            return -1;
        }

        if (non_block_flags || non_block_accept_count > 0) {
            xsink->raiseException("SOCKET-NON-BLOCK-ERROR", "a non-blocking operation is currently in progress");
            return -1;
        }

        return checkValid(xsink);
    }

    //! Throws an exception if overlapping non-blocking operations are in progress
    DLLLOCAL int checkNonBlock(ExceptionSink* xsink, unsigned direction) {
        // must be called with the lock held
        assert(m.trylock());

        // Prevent sync operations on a socket managed by the async I/O controller
        if (checkSyncAllowed(xsink)) {
            return -1;
        }

        if ((non_block_flags & direction) || non_block_accept_count > 0) {
            xsink->raiseException("SOCKET-NON-BLOCK-ERROR",
                "a non-blocking %s operation is currently in progress",
                getNonBlockDirectionName(direction));
            return -1;
        }

        return checkValid(xsink);
    }

    //! Throws a \c SOCKET-NOT-OPEN exception if the socket is not open or valid
    DLLLOCAL int checkOpen(ExceptionSink* xsink);

    //! Starts an async controller operation and claims async ownership until clearAsyncIo() is called
    DLLLOCAL int startAsyncIo(ExceptionSink* xsink) {
        // must be called with the lock held
        assert(m.trylock());

        if (checkValid(xsink)) {
            return -1;
        }
        ++async_io_count;
        return 0;
    }

#ifdef DEBUG
    //! Debug-only: waits until an operation of the socket is in the cache of its I/O thread
    /** The socket's I/O thread processes the controller's commands for the socket in order, so a command sent after
        this returns (a close, for one) is processed after the operation has been taken, and finds it on the I/O
        thread.

        @param timeout_ms a guard against a hang of the test

        @return true if the socket has one, false if the guard expired first
    */
    DLLLOCAL bool dbgWaitForControllerOp(int timeout_ms) {
        AutoLocker al(dbg_controller_ops_lock);
        int64 deadline_us = q_get_monotonic_us() + static_cast<int64>(timeout_ms) * 1000;
        while (!dbg_controller_ops) {
            int64 remaining_us = deadline_us - q_get_monotonic_us();
            if (remaining_us <= 0) {
                return false;
            }
            dbg_controller_ops_cond.wait2(&dbg_controller_ops_lock, (remaining_us + 999) / 1000);
        }
        return true;
    }

    //! Debug-only: called by the socket's I/O thread when an operation of the socket enters (+1) or leaves (-1) its
    //! cache
    DLLLOCAL void dbgNoteControllerOp(int dt) {
        AutoLocker al(dbg_controller_ops_lock);
        dbg_controller_ops += dt;
        assert(dbg_controller_ops >= 0);
        dbg_controller_ops_cond.broadcast();
    }

    //! Debug-only: the number of operations of the socket in the cache of its I/O thread
    int dbg_controller_ops = 0;
    //! Debug-only: the lock of dbg_controller_ops, a leaf lock
    QoreThreadLock dbg_controller_ops_lock;
    //! Debug-only: signaled when dbg_controller_ops changes
    QoreCondition dbg_controller_ops_cond;
#endif

    //! Starts a multi-step async controller sequence and claims async ownership until clearAsyncSequenceIo() is called
    DLLLOCAL int startAsyncSequenceIo(ExceptionSink* xsink, unsigned direction) {
        // must be called with the lock held
        assert(m.trylock());

        if (checkAsyncSequenceAllowed(xsink, direction)) {
            return -1;
        }
        if (checkValid(xsink)) {
            return -1;
        }

        int tid = q_gettid();
        unsigned flags = direction & NB_ALL;
        if (flags & NB_SEND) {
            startAsyncSequenceIoIntern(NB_SEND, tid);
        }
        if (flags & NB_RECV) {
            startAsyncSequenceIoIntern(NB_RECV, tid);
        }
        if (flags & NB_CONNECT) {
            startAsyncSequenceIoIntern(NB_CONNECT, tid);
        }
        ++async_io_count;
        return 0;
    }

    //! Clears an async controller operation claim
    DLLLOCAL void clearAsyncIo() {
        // must be called with the lock held
        assert(m.trylock());
        assert(async_io_count > 0);

        --async_io_count;
    }

    //! Clears a multi-step async controller sequence claim
    DLLLOCAL void clearAsyncSequenceIo(unsigned direction) {
        // must be called with the lock held
        assert(m.trylock());
        assert(async_io_count > 0);

        int tid = q_gettid();
        unsigned flags = direction & NB_ALL;
        if (flags & NB_SEND) {
            clearAsyncSequenceIoIntern(NB_SEND, tid);
        }
        if (flags & NB_RECV) {
            clearAsyncSequenceIoIntern(NB_RECV, tid);
        }
        if (flags & NB_CONNECT) {
            clearAsyncSequenceIoIntern(NB_CONNECT, tid);
        }

        --async_io_count;
    }

    //! Throws an exception if the socket is not open or valid or if SSL is already connected
    DLLLOCAL int checkOpenAndNotSsl(ExceptionSink* xsink);

    //! Sets all non-block flags (blocks everything)
    DLLLOCAL void setNonBlock() {
        // must be called with the lock held
        assert(m.trylock());

        assert(!non_block_flags);
        non_block_flags = NB_ALL;
    }

    //! Sets specific direction non-block flags
    DLLLOCAL void setNonBlock(unsigned direction) {
        // must be called with the lock held
        assert(m.trylock());

        assert(!(non_block_flags & direction));
        non_block_flags |= direction;
    }

    //! Checks and sets all non-block flags; also claims async ownership
    DLLLOCAL int setNonBlock(ExceptionSink* xsink) {
        // must be called with the lock held
        assert(m.trylock());

        if (checkAsyncSequenceAllowed(xsink, NB_ALL)) {
            return -1;
        }
        if (checkValid(xsink)) {
            return -1;
        }
        if (non_block_flags || non_block_accept_count > 0) {
            xsink->raiseException("SOCKET-NON-BLOCK-ERROR", "a non-blocking operation is currently in progress");
            return -1;
        }
        setNonBlock();
        return 0;
    }

    //! Checks and sets specific direction non-block flags; also claims async ownership
    DLLLOCAL int setNonBlock(ExceptionSink* xsink, unsigned direction) {
        // must be called with the lock held
        assert(m.trylock());

        if (checkAsyncSequenceAllowed(xsink, direction)) {
            return -1;
        }
        if (checkValid(xsink)) {
            return -1;
        }
        if ((non_block_flags & direction) || non_block_accept_count > 0) {
            xsink->raiseException("SOCKET-NON-BLOCK-ERROR",
                "a non-blocking %s operation is currently in progress",
                getNonBlockDirectionName(direction));
            return -1;
        }
        setNonBlock(direction);
        return 0;
    }

    //! Sets specific direction non-block flags for an async-controller operation already authorized by a sync guard
    DLLLOCAL int setNonBlockFromAsyncController(ExceptionSink* xsink, unsigned direction) {
        return setNonBlockFromAsyncController(xsink, direction, q_gettid());
    }

    //! Sets specific direction non-block flags for an async-controller operation authorized by @a tid
    DLLLOCAL int setNonBlockFromAsyncController(ExceptionSink* xsink, unsigned direction, int tid) {
        // must be called with the lock held
        assert(m.trylock());

        if (checkAsyncSequenceAllowedForTid(xsink, direction, tid)) {
            return -1;
        }
        if (checkValid(xsink)) {
            return -1;
        }
        if ((non_block_flags & direction) || non_block_accept_count > 0) {
            xsink->raiseException("SOCKET-NON-BLOCK-ERROR",
                "a non-blocking %s operation is currently in progress",
                getNonBlockDirectionName(direction));
            return -1;
        }
        setNonBlock(direction);
        return 0;
    }

    //! Clears all non-block flags
    DLLLOCAL void clearNonBlock() {
        // must be called with the lock held
        assert(m.trylock());
        non_block_flags = 0;
    }

    //! Clears specific direction non-block flags
    DLLLOCAL void clearNonBlock(unsigned direction) {
        // must be called with the lock held
        assert(m.trylock());
        non_block_flags &= ~direction;
    }

    //! Increments accept refcount (concurrent accept ops allowed); claims async ownership
    DLLLOCAL int setNonBlockAccept(ExceptionSink* xsink) {
        // must be called with the lock held
        assert(m.trylock());

        if (checkAsyncSequenceAllowed(xsink, NB_ALL)) {
            return -1;
        }
        if (non_block_flags) {
            xsink->raiseException("SOCKET-NON-BLOCK-ERROR", "a non-blocking operation is currently in progress");
            return -1;
        }
        if (checkValid(xsink)) {
            return -1;
        }
        ++non_block_accept_count;
        return 0;
    }

    //! Increments accept refcount for an async-controller operation authorized by @a tid
    DLLLOCAL int setNonBlockAcceptFromAsyncController(ExceptionSink* xsink, int tid) {
        // must be called with the lock held
        assert(m.trylock());

        if (checkAsyncSequenceAllowedForTid(xsink, NB_ALL, tid)) {
            return -1;
        }
        if (non_block_flags) {
            xsink->raiseException("SOCKET-NON-BLOCK-ERROR", "a non-blocking operation is currently in progress");
            return -1;
        }
        if (checkValid(xsink)) {
            return -1;
        }
        ++non_block_accept_count;
        return 0;
    }

    //! Decrements accept refcount
    DLLLOCAL void clearNonBlockAccept() {
        // must be called with the lock held
        assert(m.trylock());
        assert(non_block_accept_count > 0);
        --non_block_accept_count;
    }

    //! Check if the socket has an active QUIC session
    DLLLOCAL bool hasQuicSession() const;

    //! Mark every stream on every QUIC session bound to this socket as read-shut-down
    /** Iterates @c quic_sessions and calls @ref QuicSession::shutdownStreamReads()
        on each.  Used by @ref Http3ServerPollOperationPriv::abort() to break
        handler threads out of @c readQuicStreamDataBlock() during HttpServer
        shutdown — see @ref QuicSession::shutdownStreamReads for the full
        rationale.
    */
    DLLLOCAL void shutdownAllQuicStreamReads();

    //! sets backwards-compatible members on accept in a new object - will be removed in a future version of qore
    DLLLOCAL void setAccept(QoreObject* o) {
        qore_socket_ops(socket).setAccept(o);
    }

    //! sets the backwards-compatible \c source and \c source_host members of an accepted socket's object
    /** @param sock the accepted socket
        @param o the accepted socket's object
        @param host_lookup if true, the peer's hostname is resolved for \c source_host
    */
    DLLLOCAL static void setAccept(QoreSocketObject& sock, QoreObject* o, bool host_lookup);

    //! Returns the my_socket_priv pointer for a QoreSocketObject (for internal use)
    /** @since %Qore 3.0
    */
    DLLLOCAL static my_socket_priv* getPriv(QoreSocketObject& sock) {
        return sock.priv;
    }

    //! Posts a socket data event through the underlying socket private implementation
    DLLLOCAL void doDataEvent(int event, int source, const QoreStringNode& str) const;

    //! Posts a socket data event with raw bytes through the underlying socket private implementation
    DLLLOCAL void doDataEvent(int event, int source, const void* data, size_t size) const;

    //! Posts a socket header event through the underlying socket private implementation
    DLLLOCAL void doHeaderEvent(int event, int source, const QoreHashNode& hdr) const;

    //! Posts a socket HTTP chunked read event through the underlying socket private implementation
    DLLLOCAL void doChunkedReadEvent(int event, size_t bytes, size_t total_read, int source) const;

    //! Posts a socket HTTP header read event through the underlying socket private implementation
    DLLLOCAL void doReadHttpHeaderEvent(int event, const QoreHashNode& hdr, int source) const;

    //! Parses HTTP chunked body trailer lines into a hash using the underlying socket private implementation
    /** @param h the hash that the trailer fields are given in
        @param hdr the trailer text
        @param xsink for exceptions
        @param info the info hash, if any
        @param in_trailers true if \a h is the hash of a message with a body: the fields are then given under its
        \c "trailers" key, and at its top level only as the socket's field placement allows and never with a name
        that is a key of the message (qore#6761); false if \a h is a hash of trailer fields only

        @return 0 for OK, -1 if an exception was raised: the trailer text contains a NUL character
    */
    DLLLOCAL int convertHeaderToHash(QoreHashNode& h, QoreString& hdr, ExceptionSink* xsink,
            QoreHashNode* info = nullptr, bool in_trailers = false) const;

    //! Called when the read of a chunked body starts: clears the pending chunked body expectation flag, and the
    //! body is unfinished until finishHttpChunkedBodyRead() is called (issue 7417)
    DLLLOCAL void startHttpChunkedBodyRead() const;

    //! Called when the read of a chunked body has read the end of the body
    DLLLOCAL void finishHttpChunkedBodyRead() const;

    //! Returns true if the read of a chunked body started on the connection and stopped before the end of the body
    DLLLOCAL bool unfinishedHttpChunkedBody() const;

    //! Raises an exception and returns -1 if a header cannot be read, because a chunked body read on the
    //! connection stopped before its end (issue 7417)
    DLLLOCAL int checkHttpChunkedBodyFinished(ExceptionSink* xsink) const;

    //! Returns the size of a line of a chunked body read up to its line end, without the line end
    DLLLOCAL static size_t httpLinePayloadSize(const char* str, size_t len);

    //! returns true if a response with the given \c Transfer-Encoding field value has a chunked body (RFC 9112
    //! section 6.3: its final transfer coding is chunked); a response with another one is delimited by the end of the
    //! connection
    DLLLOCAL static bool isHttpResponseBodyChunked(QoreValue te);

    //! Parses the size of a chunk from its chunk size line without the line end (RFC 9112 section 7.1)
    DLLLOCAL static int parseHttpChunkSize(const char* line, size_t len, int64_t& chunk_size, ExceptionSink* xsink);

    //! Sets whether the fields of the HTTP messages read are also given at the top level of their hashes
    DLLLOCAL void setHttpFieldsAtTopLevel(bool top_level) const;

    //! Returns true if the fields of the HTTP messages read are also given at the top level of their hashes
    DLLLOCAL bool getHttpFieldsAtTopLevel() const;

    //! Returns the configured maximum chunked body size, or 0 if unlimited
    DLLLOCAL int64 getMaxChunkedBodySize() const;

    //! Returns and clears the persisted SSE CR state
    DLLLOCAL bool takeSseGotCr() const;

    //! Sets the persisted SSE CR state
    DLLLOCAL void setSseGotCr(bool got_cr) const;

    //! Builds HTTP request headers and emits the legacy HTTP send-message event
    DLLLOCAL int getSendHttpMessageHeaders(ExceptionSink* xsink, QoreString& hdr, QoreHashNode* info,
            const char* method, const char* path, const char* http_version, const QoreHashNode* headers,
            size_t size, int source) const;

    //! Builds HTTP chunked request headers and emits the legacy HTTP send-message event
    DLLLOCAL int getSendHttpMessageChunkedHeaders(ExceptionSink* xsink, QoreString& hdr, QoreHashNode* info,
            const char* method, const char* path, const char* http_version, const QoreHashNode* headers,
            int source) const;

    //! Returns true if an HTTP/2 session is active; must be called on the async I/O controller path
    DLLLOCAL bool hasH2SessionForAsyncPoll() const;

    //! Parses ALPN protocol names into caller-owned storage.
    DLLLOCAL static int parseAlpnProtocols(const QoreListNode* protocols, std::vector<std::string>& proto_list,
            ExceptionSink* xsink) {
        if (!protocols || !protocols->size()) {
            xsink->raiseException("SOCKET-ALPN-ERROR", "protocol list is empty");
            return -1;
        }

        ConstListIterator li(protocols);
        while (li.next()) {
            QoreStringValueHelper str(li.getValue());
            // a protocol name is sent in the TLS handshake as a C string, which would be cut at a NUL character
            if (q_check_no_nul(str->c_str(), str->size(), "ALPN protocol", "ALPN protocols", "SOCKET-ALPN-ERROR",
                    xsink)) {
                return -1;
            }
            if (!str->empty()) {
                proto_list.push_back(str->c_str());
            }
        }

        if (proto_list.empty()) {
            xsink->raiseException("SOCKET-ALPN-ERROR", "no valid protocols in list");
            return -1;
        }
        return 0;
    }

    //! Builds the HTTP response status line and sets the legacy response-uri info key
    DLLLOCAL void getSendHttpResponseStatusLine(QoreString& hdr, QoreHashNode* info, int code, const char* desc,
            const char* http_version) const;

    //! Builds HTTP/1 response headers and emits the legacy HTTP send-message event
    DLLLOCAL int getSendHttpResponseHeaders(ExceptionSink* xsink, QoreString& hdr, QoreHashNode* info, int code,
            const char* desc, const char* http_version, const QoreHashNode* headers, size_t size, int source) const;

    //! Builds HTTP/1 chunked response headers and emits the legacy HTTP send-message event
    DLLLOCAL int getSendHttpResponseChunkedHeaders(ExceptionSink* xsink, QoreString& hdr, QoreHashNode* info, int code,
            const char* desc, const char* http_version, const QoreHashNode* headers, int source) const;

    //! Builds HTTP/1 response headers exactly as given and emits the legacy HTTP send-message event
    /** No \c Content-Length or \c Transfer-Encoding header is added; the caller frames the message body
    */
    DLLLOCAL void getSendHttpResponseHeaderBlock(QoreString& hdr, QoreHashNode* info, int code, const char* desc,
            const char* http_version, const QoreHashNode* headers, int source) const;

private:
    DLLLOCAL int checkAsyncSequenceAllowedIntern(ExceptionSink* xsink, unsigned direction, int tid) const {
        int index = getAsyncSequenceIndex(direction);
        if (async_sequence_count[index] && async_sequence_owner_tid[index] != tid) {
            xsink->raiseException("SOCKET-ASYNC-MODE-ERROR",
                "cannot start an async %s operation while TID %d owns a multi-step async %s operation on the socket",
                getNonBlockDirectionName(direction), async_sequence_owner_tid[index],
                getNonBlockDirectionName(direction));
            return -1;
        }
        return 0;
    }

    DLLLOCAL void startAsyncSequenceIoIntern(unsigned direction, int tid) {
        int index = getAsyncSequenceIndex(direction);
        assert(!async_sequence_count[index] || async_sequence_owner_tid[index] == tid);
        if (!async_sequence_count[index]) {
            async_sequence_owner_tid[index] = tid;
        }
        ++async_sequence_count[index];
    }

    DLLLOCAL void clearAsyncSequenceIoIntern(unsigned direction, int tid) {
        int index = getAsyncSequenceIndex(direction);
        assert(async_sequence_count[index] > 0);
        assert(async_sequence_owner_tid[index] == tid);
        --async_sequence_count[index];
        if (!async_sequence_count[index]) {
            async_sequence_owner_tid[index] = -1;
        }
    }
};

//! The guard that every non-static method of Socket calls first (the class's "guard" property; qore#7338)
/** One pointer test for every method of every object; the sandbox is resolved only on the connection socket of a
    server (see Socket::setServerOwned()), at each call, never cached

    @param self the object whose method is called
    @param s the object's private data
    @param kind 0 for a method that reads, writes, changes or polls the connection, 1 for close() and shutdown*()
    @param xsink for the exception

    @return 0 if the call is allowed, -1 if not (the exception is raised)
*/
static inline int qore_socket_server_guard(QoreObject* self, QoreSocketObject* s, int kind, ExceptionSink* xsink) {
    if (my_socket_priv::getPriv(*s)->server_owned.load(std::memory_order_acquire) != self) {
        return 0;
    }
    return qore_socket_server_guard_check(kind, xsink);
}

//! The guard of a pollable I/O object that code other than its own methods uses (qore#7338)
/** For the parameters of other classes' methods and functions that take a Socket (or any pollable I/O object) and act
    on it (poll operations, PollPipeline, EventLoop, AsyncIoController, Socket::poll()): the connection socket of a
    server is refused to sandboxed code there as in its own methods

    @param obj the object given
    @param p its private data as a pollable I/O object
    @param xsink for the exception

    @return 0 if the use is allowed, -1 if not (the exception is raised)
*/
static inline int qore_pollable_server_guard(const QoreObject* obj, AbstractPollableIoObjectBase* p,
        ExceptionSink* xsink) {
    if (!obj || !p || !obj->getClass(CID_SOCKET)) {
        return 0;
    }
    // the object's own Socket private data: a class can inherit another pollable class before Socket, so the pollable
    // private data is not necessarily the Socket's
    TryPrivateDataRefHolder<QoreSocketObject> sock(obj, CID_SOCKET, xsink);
    if (!sock) {
        return *xsink ? -1 : 0;
    }
    return qore_socket_server_guard(const_cast<QoreObject*>(obj), *sock, 0, xsink);
}

#endif // _QORE_CLASS_QORESOCKET_H

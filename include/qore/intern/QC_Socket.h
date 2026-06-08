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

#include <qore/QoreSocket.h>
#include <qore/AbstractPrivateData.h>
#include <qore/QoreThreadLock.h>
#include <qore/QoreSocketObject.h>
#include <qore/qore_thread.h>
#include "qore/intern/QC_SSLCertificate.h"
#include "qore/intern/QC_SSLPrivateKey.h"

class my_socket_priv {
public:
    QoreSocket* socket;
    QoreSSLCertificate* cert = nullptr;
    QoreSSLPrivateKey* pk = nullptr;
    mutable QoreThreadLock m;
    std::string ssl_cipher_name_cache;
    std::string ssl_cipher_version_cache;
    unsigned non_block_flags = 0;
    int non_block_accept_count = 0;
    int async_io_count = 0;
    int async_sequence_owner_tid[3] = {-1, -1, -1};
    int async_sequence_count[3] = {0, 0, 0};
    bool valid = true;

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
        socket->setAccept(o);
    }

    DLLLOCAL static void setAccept(QoreSocketObject& sock, QoreObject* o);

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

    //! Parses HTTP header lines into a hash using the underlying socket private implementation
    DLLLOCAL void convertHeaderToHash(QoreHashNode& h, QoreString& hdr, QoreHashNode* info = nullptr) const;

    //! Clears the pending chunked body expectation flag on the underlying socket private implementation
    DLLLOCAL void clearHttpExpectChunkedBody() const;

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

    //! Returns the active HTTP/2 server stream ID for this thread, or <= 0 if no HTTP/2 response is active
    DLLLOCAL int32_t getH2ActiveServerStreamId() const;

    //! Returns the thread-local HTTP/2 stream ID without inspecting the HTTP/2 session
    DLLLOCAL int32_t getH2ActiveThreadStreamId() const;

    //! Returns true if an HTTP/2 session is active; must be called on the async I/O controller path
    DLLLOCAL bool hasH2SessionForAsyncPoll() const;

    //! Returns true if an HTTP/2 server session is active; must be called on the async I/O controller path
    DLLLOCAL bool isH2ServerSessionForAsyncPoll() const;

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

#endif // _QORE_CLASS_QORESOCKET_H

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
#include "qore/intern/QC_SSLCertificate.h"
#include "qore/intern/QC_SSLPrivateKey.h"

class my_socket_priv {
public:
    QoreSocket* socket;
    QoreSSLCertificate* cert = nullptr;
    QoreSSLPrivateKey* pk = nullptr;
    mutable QoreThreadLock m;
    unsigned non_block_flags = 0;
    int non_block_accept_count = 0;
    int async_io_count = 0;
    int sync_io_count = 0;
    bool valid = true;
    SocketIoMode io_mode = SocketIoMode::Unclaimed;

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

    //! Throws an exception if the socket is in Async mode and a sync operation is attempted
    /** @return 0 if sync I/O is allowed, -1 if not (exception raised on @a xsink)
        @since %Qore 2.3
    */
    DLLLOCAL int checkSyncAllowed(ExceptionSink* xsink) {
        // must be called with the lock held
        assert(m.trylock());

        if (io_mode == SocketIoMode::Async || async_io_count > 0) {
            xsink->raiseException("SOCKET-ASYNC-MODE-ERROR",
                "cannot perform synchronous I/O on a socket managed by "
                "the async I/O controller");
            return -1;
        }
        return 0;
    }

    //! Throws an exception if the socket is in Sync mode and an async operation is attempted
    /** @return 0 if async I/O is allowed, -1 if not (exception raised on @a xsink)
        @since %Qore 2.3
    */
    DLLLOCAL int checkAsyncAllowed(ExceptionSink* xsink) {
        // must be called with the lock held
        assert(m.trylock());

        if (io_mode == SocketIoMode::Sync || sync_io_count > 0) {
            xsink->raiseException("SOCKET-SYNC-MODE-ERROR",
                "cannot perform async I/O on a socket with active "
                "synchronous operations");
            return -1;
        }
        return 0;
    }

    //! Sets the I/O mode of the socket
    /** @since %Qore 2.3
    */
    DLLLOCAL void setIoMode(SocketIoMode mode) {
        // must be called with the lock held
        assert(m.trylock());
        io_mode = mode;
    }

    //! Returns the current I/O mode of the socket
    /** @since %Qore 2.3
    */
    DLLLOCAL SocketIoMode getIoMode() const {
        // must be called with the lock held
        assert(m.trylock());
        return io_mode;
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
                direction == NB_SEND ? "send" : direction == NB_RECV ? "receive" : "connect");
            return -1;
        }

        return checkValid(xsink);
    }

    //! Throws a \c SOCKET-NOT-OPEN exception if the socket is not open or valid
    DLLLOCAL int checkOpen(ExceptionSink* xsink);

    //! Starts a synchronous I/O operation and claims sync I/O mode until clearSyncIo() is called
    DLLLOCAL int startSyncIo(ExceptionSink* xsink) {
        // must be called with the lock held
        assert(m.trylock());

        if (checkNonBlock(xsink)) {
            return -1;
        }
        ++sync_io_count;
        io_mode = SocketIoMode::Sync;
        return 0;
    }

    //! Starts a directional synchronous I/O operation and claims sync I/O mode until clearSyncIo() is called
    DLLLOCAL int startSyncIo(ExceptionSink* xsink, unsigned direction) {
        // must be called with the lock held
        assert(m.trylock());

        if (checkNonBlock(xsink, direction)) {
            return -1;
        }
        ++sync_io_count;
        io_mode = SocketIoMode::Sync;
        return 0;
    }

    //! Starts an async controller operation and claims async I/O mode until clearAsyncIo() is called
    DLLLOCAL int startAsyncIo(ExceptionSink* xsink) {
        // must be called with the lock held
        assert(m.trylock());

        if (checkAsyncAllowed(xsink)) {
            return -1;
        }
        if (checkValid(xsink)) {
            return -1;
        }
        ++async_io_count;
        io_mode = SocketIoMode::Async;
        return 0;
    }

    //! Clears an async controller operation claim
    DLLLOCAL void clearAsyncIo() {
        // must be called with the lock held
        assert(m.trylock());
        assert(async_io_count > 0);

        --async_io_count;
        if (!async_io_count && !non_block_flags && non_block_accept_count == 0
                && io_mode == SocketIoMode::Async) {
            io_mode = SocketIoMode::Unclaimed;
        }
    }

    //! Clears a synchronous I/O operation claim
    DLLLOCAL void clearSyncIo() {
        // must be called with the lock held
        assert(m.trylock());
        assert(sync_io_count > 0);

        --sync_io_count;
        if (!sync_io_count && io_mode == SocketIoMode::Sync) {
            if (async_io_count) {
                io_mode = SocketIoMode::Async;
            } else if (!non_block_flags && non_block_accept_count == 0) {
                io_mode = SocketIoMode::Unclaimed;
            }
        }
    }

    class SyncIoGuard {
    public:
        DLLLOCAL SyncIoGuard(my_socket_priv& p, ExceptionSink* xsink) : p(p) {
            active = !p.startSyncIo(xsink);
        }

        DLLLOCAL SyncIoGuard(my_socket_priv& p, ExceptionSink* xsink, unsigned direction) : p(p) {
            active = !p.startSyncIo(xsink, direction);
        }

        DLLLOCAL ~SyncIoGuard() {
            if (active) {
                p.clearSyncIo();
            }
        }

        DLLLOCAL explicit operator bool() const {
            return active;
        }

        SyncIoGuard(const SyncIoGuard&) = delete;
        SyncIoGuard& operator=(const SyncIoGuard&) = delete;

    private:
        my_socket_priv& p;
        bool active = false;
    };

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

    //! Checks and sets all non-block flags; also claims async I/O mode
    DLLLOCAL int setNonBlock(ExceptionSink* xsink) {
        // must be called with the lock held
        assert(m.trylock());

        if (checkAsyncAllowed(xsink)) {
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
        io_mode = SocketIoMode::Async;
        return 0;
    }

    //! Checks and sets specific direction non-block flags; also claims async I/O mode
    DLLLOCAL int setNonBlock(ExceptionSink* xsink, unsigned direction) {
        // must be called with the lock held
        assert(m.trylock());

        if (checkAsyncAllowed(xsink)) {
            return -1;
        }
        if (checkValid(xsink)) {
            return -1;
        }
        if ((non_block_flags & direction) || non_block_accept_count > 0) {
            xsink->raiseException("SOCKET-NON-BLOCK-ERROR",
                "a non-blocking %s operation is currently in progress",
                direction == NB_SEND ? "send" : direction == NB_RECV ? "receive" : "connect");
            return -1;
        }
        setNonBlock(direction);
        io_mode = SocketIoMode::Async;
        return 0;
    }

    //! Clears all non-block flags and resets I/O mode to Unclaimed
    DLLLOCAL void clearNonBlock() {
        // must be called with the lock held
        assert(m.trylock());
        non_block_flags = 0;
        if (!async_io_count && non_block_accept_count == 0) {
            io_mode = SocketIoMode::Unclaimed;
        }
    }

    //! Clears specific direction non-block flags; resets I/O mode when all flags clear
    DLLLOCAL void clearNonBlock(unsigned direction) {
        // must be called with the lock held
        assert(m.trylock());
        non_block_flags &= ~direction;
        if (!async_io_count && !non_block_flags && non_block_accept_count == 0) {
            io_mode = SocketIoMode::Unclaimed;
        }
    }

    //! Increments accept refcount (concurrent accept ops allowed); claims async I/O mode
    DLLLOCAL int setNonBlockAccept(ExceptionSink* xsink) {
        // must be called with the lock held
        assert(m.trylock());

        if (checkAsyncAllowed(xsink)) {
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
        io_mode = SocketIoMode::Async;
        return 0;
    }

    //! Decrements accept refcount; resets I/O mode when no async ops remain
    DLLLOCAL void clearNonBlockAccept() {
        // must be called with the lock held
        assert(m.trylock());
        assert(non_block_accept_count > 0);
        --non_block_accept_count;
        if (!async_io_count && !non_block_flags && non_block_accept_count == 0) {
            io_mode = SocketIoMode::Unclaimed;
        }
    }

    //! Check if the socket has an active QUIC session
    DLLLOCAL bool hasQuicSession() const;

    //! sets backwards-compatible members on accept in a new object - will be removed in a future version of qore
    DLLLOCAL void setAccept(QoreObject* o) {
        socket->setAccept(o);
    }

    DLLLOCAL static void setAccept(QoreSocketObject& sock, QoreObject* o) {
        sock.priv->setAccept(o);
    }

    //! Returns the my_socket_priv pointer for a QoreSocketObject (for internal use)
    /** @since %Qore 2.3
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

    //! Builds the HTTP response status line and sets the legacy response-uri info key
    DLLLOCAL void getSendHttpResponseStatusLine(QoreString& hdr, QoreHashNode* info, int code, const char* desc,
            const char* http_version) const;

    //! Builds HTTP/1 response headers and emits the legacy HTTP send-message event
    DLLLOCAL int getSendHttpResponseHeaders(ExceptionSink* xsink, QoreString& hdr, QoreHashNode* info, int code,
            const char* desc, const char* http_version, const QoreHashNode* headers, size_t size, int source) const;

    //! Builds HTTP/1 chunked response headers and emits the legacy HTTP send-message event
    DLLLOCAL int getSendHttpResponseChunkedHeaders(ExceptionSink* xsink, QoreString& hdr, QoreHashNode* info, int code,
            const char* desc, const char* http_version, const QoreHashNode* headers, int source) const;
};

#endif // _QORE_CLASS_QORESOCKET_H

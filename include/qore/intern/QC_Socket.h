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

    //! Throws an exception if any non-blocking operation is in progress or is not valid
    DLLLOCAL int checkNonBlock(ExceptionSink* xsink) {
        // must be called with the lock held
        assert(m.trylock());

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

    //! Checks and sets all non-block flags
    DLLLOCAL int setNonBlock(ExceptionSink* xsink) {
        // must be called with the lock held
        assert(m.trylock());

        if (!checkNonBlock(xsink)) {
            setNonBlock();
            return 0;
        }
        return -1;
    }

    //! Checks and sets specific direction non-block flags
    DLLLOCAL int setNonBlock(ExceptionSink* xsink, unsigned direction) {
        // must be called with the lock held
        assert(m.trylock());

        if (!checkNonBlock(xsink, direction)) {
            setNonBlock(direction);
            return 0;
        }
        return -1;
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

    //! Increments accept refcount (concurrent accept ops allowed)
    DLLLOCAL int setNonBlockAccept(ExceptionSink* xsink) {
        // must be called with the lock held
        assert(m.trylock());

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

    //! sets backwards-compatible members on accept in a new object - will be removed in a future version of qore
    DLLLOCAL void setAccept(QoreObject* o) {
        socket->setAccept(o);
    }

    DLLLOCAL static void setAccept(QoreSocketObject& sock, QoreObject* o) {
        sock.priv->setAccept(o);
    }
};

#endif // _QORE_CLASS_QORESOCKET_H

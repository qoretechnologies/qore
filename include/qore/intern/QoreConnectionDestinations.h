/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreConnectionDestinations.h

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

#ifndef _QORE_INTERN_QORECONNECTIONDESTINATIONS_H
#define _QORE_INTERN_QORECONNECTIONDESTINATIONS_H

#include <qore/common.h>
#include <qore/QoreSandboxManager.h>

#include <string>
#include <vector>

class ExceptionSink;

//! Where a network connection leads: the destinations that a network sandbox decides on
/** A connection is checked against the network sandbox of the code that makes it.  A connection pool hands an open
    connection to later callers, whose code can run under another sandbox; before it does, it checks the connection's
    destinations against that sandbox as a new connection to them would be checked (see
    HttpClientConnectionBase::checkSandboxUse()).

    The asynchronous connect operations of a socket record the destinations when the connection is made; they do not
    change afterwards.

    @since %Qore 3.0
*/
class QoreConnectionDestinations {
public:
    //! Adds a destination that the client resolved
    /** @param host the host name or address literal that @a addr was resolved from, or the path of a UNIX socket
        @param addr the address
        @param addrlen the length of @a addr
        @param proto the protocol: @ref QSEC_NET_TCP, @ref QSEC_NET_UDP or @ref QSEC_NET_UNIX
    */
    DLLLOCAL void add(const char* host, const struct sockaddr* addr, socklen_t addrlen, int proto);

    //! Adds a destination that the client did not resolve
    /** A proxy resolves the destination of a request itself; the client resolves it only to check it against the
        sandbox that governs the connection, so a connection made without a sandbox leaves the destination
        unresolved, and no sandbox can decide on it.

        @param host the host name of the destination
        @param proto the protocol: @ref QSEC_NET_TCP
    */
    DLLLOCAL void addUnresolved(const char* host, int proto);

    //! Checks the destinations against a network sandbox, as a new connection to them would be checked
    /** @param sm the sandbox
        @param xsink raises \c NETWORK-ACCESS-DENIED if the sandbox denies a destination

        @return 0 if the sandbox allows every destination; -1 if it denies one (an exception is raised); 1 if no
        destination is denied but one is unresolved, so the sandbox cannot decide on it (no exception is raised)
    */
    DLLLOCAL int check(QoreSandboxManager& sm, ExceptionSink* xsink) const;

private:
    struct Destination {
        //! the host name or address literal, or the path of a UNIX socket
        std::string host;
        //! the address; not set for an unresolved destination
        struct sockaddr_storage addr = {};
        //! the length of the address; 0 for an unresolved destination
        socklen_t addrlen = 0;
        //! the protocol (QSEC_NET_*)
        int proto = 0;
    };

    std::vector<Destination> destinations;
};

#endif

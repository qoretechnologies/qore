# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file HttpClientCallerExemption.qc the caller check of the connections of HttpClientIo

/** HttpClientCallerExemption.qc Copyright 2026 Qore Technologies, s.r.o.

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
*/

#! Main namespace
public namespace HttpClientIo {

#! The access of this module's code to the socket of a poll operation (qore#7544)
/** Not exported: only this module's code can name it, and so get its token.  The socket of a connection carries the
    requests of every caller of its connection manager: a poll operation gives it with
    @ref HttpClientIo::HttpClientPollOperation::getSocket() "getSocket()" to code that a sandbox restricts only for a
    connection of its own (see @ref HttpClientIo::HttpClientConnection::getSocket() "HttpClientConnection::getSocket()"),
    while this module's code, which uses the socket for every caller, takes it with
    @ref HttpClientIo::HttpClientPollOperation::getSocketFor() "getSocketFor()" and this class's token.
*/
class HttpClientIoSocketAccess {
    private:internal {
        #! the token
        static object token = new Mutex();
    }

    #! Returns the socket of a poll operation for the code of this module
    /** The token is given only to an operation of one of the poll operation classes of this module, whose methods
        that take it are final (Http1ClientPollOperation::getSocketFor(), Http2ClientPollOperation::getSocketFor(),
        Http3ClientPollOperation::getSocketFor(), HttpClientPollOperation::clearConnection()), so that no subclass can
        keep it; another operation (of a class that other code derives from HttpClientPollOperation) gives its socket
        with its public accessor, which applies the rules of the connection for the calling code (qore#7544)
    */
    static Socket socketOf(HttpClientPollOperation op) {
        if (HttpClientIoSocketAccess::isModuleOperation(op)) {
            return op.getSocketFor(token);
        }
        return op.getSocket();
    }

    #! Releases the connection of a poll operation for the code of this module; see socketOf()
    /** @param op the poll operation
        @param conn the connection that is closed: the operation releases only its own connection
    */
    static clearConnectionOf(HttpClientPollOperation op, HttpClientConnection conn) {
        if (HttpClientIoSocketAccess::isModuleOperation(op)) {
            op.clearConnection(token, conn);
        }
    }

    #! Returns @ref True if the operation is of a poll operation class of this module (or a subclass of one)
    private:internal static bool isModuleOperation(HttpClientPollOperation op) {
        return op instanceof Http1ClientPollOperation || op instanceof Http2ClientPollOperation
            || op instanceof Http3ClientPollOperation;
    }

    #! Raises an exception if the given object is not the token
    /** @throw PERMISSION-DENIED the object is not the token
    */
    static check(object access) {
        if (access !== token) {
            throw "PERMISSION-DENIED", "this method of an HTTP client poll operation is reserved for the code of the "
                "HttpClientIo module";
        }
    }
}

#! The caller check of the connections and poll operations of the module, and its exemption (qore#6981)
/** Not exported: only this module's code can create one.  A connection
    (@ref HttpClientIo::Http1ClientConnection "Http1ClientConnection", @ref HttpClientIo::Http2ClientConnection
    "Http2ClientConnection", @ref HttpClientIo::Http3ClientConnection "Http3ClientConnection") and a poll operation
    (@ref HttpClientIo::Http1ClientPollOperation "Http1ClientPollOperation", @ref HttpClientIo::Http2ClientPollOperation
    "Http2ClientPollOperation", @ref HttpClientIo::Http3ClientPollOperation "Http3ClientPollOperation") connect to the
    host that they are given and read the CA certificate locations that they are given, so code restricted from the
    \c NETWORK functional domain (or from \c FILESYSTEM, with CA locations) cannot create one (check()).

    The connection manager, which checks its caller when it is created, creates its connections, and a connection its
    poll operation, with an exemption: an object of this class, which exempts the next check on its thread, so a
    manager that the host creates and lends to restricted code is a capability that connects for it.  The exemption
    is kept in a static member of this class, by thread, so code outside the class can neither read nor create it (a
    class that other code declares with the same name and signature has static members of its own); the check takes
    it, so it covers one creation only, and the object removes it when it goes out of scope if it was not taken, also
    when an exception is raised.  The manager creates it just before it creates a connection, after evaluating
    everything that the connection is given, and a connection checks its caller before it runs any other code (its
    logger, for example), so no code of a caller runs while an exemption is held.
*/
class HttpClientCallerExemption {
    private:internal {
        #! the token of this exemption; the exemption of the thread while it is held
        object token = new Mutex();

        #! the key of this exemption's thread
        string tid = string(gettid());

        #! the exemptions held, by thread: the token of the exemption and the ID of the Program that owns the
        #! connection that it is for, if known (qore#7544); the object itself is not held, so that it is destroyed
        #! when it goes out of scope
        static hash<string, hash<auto>> active = {};

        #! the lock of \c active
        static Mutex lock();
    }

    #! Grants the exemption to the next check in this thread
    /** @param owner_pgm_id the ID of the Program that owns the connection that is created with the exemption: the
        owner of the connection manager that creates it (qore#7544)
    */
    constructor(*int owner_pgm_id) {
        AutoLock al(lock);
        active{tid} = {"token": token, "owner": owner_pgm_id};
    }

    #! Removes the exemption if it was not taken
    destructor() {
        AutoLock al(lock);
        if (active{tid}.token === token) {
            remove active{tid};
        }
    }

    #! Refuses a caller restricted from the functional domains of a connection, unless an exemption is held
    /** @param ssl_ca_file the CA certificate file of the connection, if any
        @param ssl_ca_path the CA certificate directory of the connection, if any

        @return the ID of the Program that owns the connection, if known: the owner of the connection manager that
        holds the exemption, else the Program of the calling code (qore#7544)

        @throw PERMISSION-DENIED the calling code is restricted from \c NETWORK, or from \c FILESYSTEM and CA
        locations are given
    */
    static *int check(*string ssl_ca_file, *string ssl_ca_path) {
        *hash<auto> exemption = HttpClientCallerExemption::take();
        if (exemption) {
            return exemption.owner;
        }
        if (SandboxManager::isCallerRestricted(DOM_NETWORK)) {
            throw "PERMISSION-DENIED", "the calling code may not create an HTTP client connection, as it is "
                "restricted from the NETWORK functional domain";
        }
        if ((ssl_ca_file || ssl_ca_path) && SandboxManager::isCallerRestricted(DOM_FILESYSTEM)) {
            throw "PERMISSION-DENIED", "the calling code may not give an HTTP client connection CA certificate "
                "locations, as it is restricted from the FILESYSTEM functional domain";
        }
        return SandboxManager::getCallerProgramId();
    }

    #! Takes the exemption of the thread, if one is held
    private:internal static *hash<auto> take() {
        AutoLock al(lock);
        return remove active{string(gettid())};
    }
}
}

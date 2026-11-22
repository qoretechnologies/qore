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

        #! the exemptions held, by thread
        static hash<string, object> active = {};

        #! the lock of \c active
        static Mutex lock();
    }

    #! Grants the exemption to the next check in this thread
    constructor() {
        AutoLock al(lock);
        active{tid} = token;
    }

    #! Removes the exemption if it was not taken
    destructor() {
        AutoLock al(lock);
        if (active{tid} === token) {
            remove active{tid};
        }
    }

    #! Refuses a caller restricted from the functional domains of a connection, unless an exemption is held
    /** @param ssl_ca_file the CA certificate file of the connection, if any
        @param ssl_ca_path the CA certificate directory of the connection, if any

        @throw PERMISSION-DENIED the calling code is restricted from \c NETWORK, or from \c FILESYSTEM and CA
        locations are given
    */
    static check(*string ssl_ca_file, *string ssl_ca_path) {
        if (HttpClientCallerExemption::take()) {
            return;
        }
        if (SandboxManager::isCallerRestricted(DOM_NETWORK)) {
            throw "PERMISSION-DENIED", "the calling code may not create an HTTP client connection, as it is "
                "restricted from the NETWORK functional domain";
        }
        if ((ssl_ca_file || ssl_ca_path) && SandboxManager::isCallerRestricted(DOM_FILESYSTEM)) {
            throw "PERMISSION-DENIED", "the calling code may not give an HTTP client connection CA certificate "
                "locations, as it is restricted from the FILESYSTEM functional domain";
        }
    }

    #! Takes the exemption of the thread; returns @ref True if one was held
    private:internal static bool take() {
        AutoLock al(lock);
        return exists remove active{string(gettid())};
    }
}
}

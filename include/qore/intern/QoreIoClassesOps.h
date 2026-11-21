/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreIoClassesOps.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_INTERN_QOREIOCLASSESOPS_H
#define _QORE_INTERN_QOREIOCLASSESOPS_H

#include <qore/QoreSocket.h>
#include <qore/QoreFile.h>
#include <qore/QoreFtpClient.h>
#include <qore/QoreQueue.h>

#include <utility>

/** The non-virtual DLLLOCAL member functions of QoreSocket, SocketSource, QoreFile, QoreFtpClient and QoreQueue were declared in the public headers although no module can call them (they are
    not exported); they are members of the classes below, in the implementation of the library.  Each class is a friend
    of the class it serves; call an operation through a temporary object:

    @code
    qore_socket_ops(sock).getDescriptor();
    @endcode

    A function that stays out of line is a static fooImpl(Cls* b, ...) with an inline wrapper, so that the temporary is
    optimized away and the call costs what the member function cost (a temporary whose address a call takes makes GCC
    add a stack-protector canary to the caller).
*/

//! The library-internal operations of QoreSocket (qore#4380)
class qore_socket_ops {
public:
    //! the object
    QoreSocket* b;

    DLLLOCAL explicit qore_socket_ops(QoreSocket* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_socket_ops(H& holder) : qore_socket_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_socket_ops(const QoreSocket* n_b) : b(const_cast<QoreSocket*>(n_b)) {
    }

    DLLLOCAL explicit qore_socket_ops(QoreSocket& r) : b(&r) {
    }

    DLLLOCAL explicit qore_socket_ops(const QoreSocket& r) : b(const_cast<QoreSocket*>(&r)) {
    }

    //! Starts a non-blocking TLS connection carried by the TLS connection that the socket already has
    /** Used for a TLS connection to a server through a tunnel of an HTTPS proxy, where the socket's TLS connection
        is the connection to the proxy

        @param xsink if an error occurs, the Qore-language exception information will be added here
        @param cert the client certificate to use
        @param pkey the private key to use

        @return a socket poll state object or nullptr in case of an exception

        @since %Qore 3.0
    */
    DLLLOCAL static AbstractPollState* startLayeredSslConnectImpl(QoreSocket* b, ExceptionSink* xsink, QoreSSLCertificate* cert, QoreSSLPrivateKey* pkey);
    DLLLOCAL AbstractPollState* startLayeredSslConnect(ExceptionSink* xsink, QoreSSLCertificate* cert = nullptr, QoreSSLPrivateKey* pkey = nullptr) {
        return startLayeredSslConnectImpl(b, xsink, cert, pkey);
    }

    //! Starts a non-blocking TLS connection to an HTTPS proxy
    /** Unlike startSslConnect(), the connection is made without the socket's client certificate and ALPN protocols,
        which are meant for the target server reached through the proxy

        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return a socket poll state object or nullptr in case of an exception

        @since %Qore 3.0
    */
    DLLLOCAL static AbstractPollState* startProxySslConnectImpl(QoreSocket* b, ExceptionSink* xsink);
    DLLLOCAL AbstractPollState* startProxySslConnect(ExceptionSink* xsink) {
        return startProxySslConnectImpl(b, xsink);
    }

    //! Creates a new accepted socket with listener SSL and protocol parameters copied
    DLLLOCAL static QoreSocket* createAcceptedSocketImpl(QoreSocket* b, int descriptor);
    DLLLOCAL QoreSocket* createAcceptedSocket(int descriptor) {
        return createAcceptedSocketImpl(b, descriptor);
    }

    //! returns the event queue (not part of the library's public API)
    DLLLOCAL static Queue* getQueueImpl(QoreSocket* b);
    DLLLOCAL Queue* getQueue() {
        return getQueueImpl(b);
    }

    //! Returns the socket's descriptor, for the library's own use
    DLLLOCAL static int getDescriptorImpl(QoreSocket* b);
    DLLLOCAL int getDescriptor() {
        return getDescriptorImpl(b);
    }

    DLLLOCAL static int getNoDelayImpl(QoreSocket* b);
    DLLLOCAL int getNoDelay() {
        return getNoDelayImpl(b);
    }

    //! Returns the TCP_USER_TIMEOUT value in milliseconds.
    /** When the socket is an open TCP socket and the platform supports
        @c TCP_USER_TIMEOUT, returns the kernel's current value via getsockopt.
        Otherwise returns the configured value stored on the socket (0 if
        unset).
    */
    DLLLOCAL static int getUserTimeoutImpl(QoreSocket* b);
    DLLLOCAL int getUserTimeout() {
        return getUserTimeoutImpl(b);
    }

    DLLLOCAL static int setNoDelayImpl(QoreSocket* b, int nodelay);
    DLLLOCAL int setNoDelay(int nodelay) {
        return setNoDelayImpl(b, nodelay);
    }

    //! Set the TCP_USER_TIMEOUT socket option (Linux/Solaris) in milliseconds.
    /** Kernel-level safety net for TCP-layer death.  Bounds how long
        unacknowledged data on a TCP connection may sit before the kernel
        reports the connection dead with ETIMEDOUT on the next read/write.
        Catches peer reboots, NIC failures, network partitions, and NAT
        rebinds that would otherwise hang send()/recv() for the kernel's
        full TCP retransmit budget (~15 minutes default on Linux).

        @note This is NOT a half-open HTTP detector.  If the peer's TCP
        stack ack's bytes but the application stops generating responses,
        TCP_USER_TIMEOUT never fires (nothing is unack'd).  Use an
        application-level deadline (e.g. the connection manager's
        @c request_timeout) for that case.

        The value is also stored on the socket object and re-applied after
        every successful connect, so callers may set it before connecting.
        Passing 0 disables the option (kernel default).

        On platforms without TCP_USER_TIMEOUT (BSD/macOS/Windows) the value
        is stored but no kernel option is set; the call returns 0.

        @param ms timeout in milliseconds; 0 disables
        @return 0 on success, non-zero on setsockopt failure (errno set)
    */
    DLLLOCAL static int setUserTimeoutImpl(QoreSocket* b, int ms);
    DLLLOCAL int setUserTimeout(int ms) {
        return setUserTimeoutImpl(b, ms);
    }

    //! returns a unique ID for the socket to be used in event messages
    DLLLOCAL static int64 getObjectIDForEventsImpl(QoreSocket* b);
    DLLLOCAL int64 getObjectIDForEvents() {
        return getObjectIDForEventsImpl(b);
    }

    DLLLOCAL static void doException(int rc, const char* meth, int timeout_ms, ExceptionSink* xsink);

    //! sets backwards-compatible members on accept in a new object - will be removed in a future version of qore
    DLLLOCAL static void setAcceptImpl(QoreSocket* b, QoreObject* o);
    DLLLOCAL void setAccept(QoreObject* o) {
        setAcceptImpl(b, o);
    }

    //! sets the event queue (not part of the library's pubilc API), must be already referenced before call
    DLLLOCAL static void setEventQueueImpl(QoreSocket* b, ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data);
    DLLLOCAL void setEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
        setEventQueueImpl(b, xsink, q, arg, with_data);
    }

};

//! The library-internal operations of SocketSource (qore#4380)
class qore_socket_source_ops {
public:
    //! the object
    SocketSource* b;

    DLLLOCAL explicit qore_socket_source_ops(SocketSource* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_socket_source_ops(H& holder) : qore_socket_source_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_socket_source_ops(const SocketSource* n_b) : b(const_cast<SocketSource*>(n_b)) {
    }

    DLLLOCAL explicit qore_socket_source_ops(SocketSource& r) : b(&r) {
    }

    DLLLOCAL explicit qore_socket_source_ops(const SocketSource& r) : b(const_cast<SocketSource*>(&r)) {
    }

    DLLLOCAL static void setAllImpl(SocketSource* b, QoreObject* o, ExceptionSink* xsink);
    DLLLOCAL void setAll(QoreObject* o, ExceptionSink* xsink) {
        setAllImpl(b, o, xsink);
    }

};

//! The library-internal operations of QoreFile (qore#4380)
class qore_file_ops {
public:
    //! the object
    QoreFile* b;

    DLLLOCAL explicit qore_file_ops(QoreFile* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_file_ops(H& holder) : qore_file_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_file_ops(const QoreFile* n_b) : b(const_cast<QoreFile*>(n_b)) {
    }

    DLLLOCAL explicit qore_file_ops(QoreFile& r) : b(&r) {
    }

    DLLLOCAL explicit qore_file_ops(const QoreFile& r) : b(const_cast<QoreFile*>(&r)) {
    }

    //! internal API: reads until the given end-of-line marker, converted to the file's encoding, is read
    /** @param eol the end-of-line marker, in any encoding
        @param incl_eol if true, the marker is included in the string returned
        @param xsink for exceptions

        @return the string read, or nullptr at the end of the file or if an exception was raised
    */
    DLLLOCAL static QoreStringNode* readUntilTextImpl(QoreFile* b, const QoreString& eol, bool incl_eol, ExceptionSink* xsink);
    DLLLOCAL QoreStringNode* readUntilText(const QoreString& eol, bool incl_eol, ExceptionSink* xsink) {
        return readUntilTextImpl(b, eol, incl_eol, xsink);
    }

    //! gets terminal attributes
    DLLLOCAL static int getTerminalAttributesImpl(QoreFile* b, QoreTermIOS *ios, ExceptionSink* xsink);
    DLLLOCAL int getTerminalAttributes(QoreTermIOS *ios, ExceptionSink* xsink) {
        return getTerminalAttributesImpl(b, ios, xsink);
    }

    //! opens the file like open() if the sandbox filesystem policy allows it (not part of the library's public API)
    /** @return 0 for success, -1 for error; if the sandbox policy denies access, an exception is raised, otherwise
        \c errno is set as for open()
    */
    DLLLOCAL static int openCheckAccessImpl(QoreFile* b, ExceptionSink* xsink, const char* fn, int flags, int mode, const QoreEncoding* cs);
    DLLLOCAL int openCheckAccess(ExceptionSink* xsink, const char* fn, int flags, int mode, const QoreEncoding* cs) {
        return openCheckAccessImpl(b, xsink, fn, flags, mode, cs);
    }

    //! sets terminal attributes
    DLLLOCAL static int setTerminalAttributesImpl(QoreFile* b, int action, QoreTermIOS *ios, ExceptionSink* xsink);
    DLLLOCAL int setTerminalAttributes(int action, QoreTermIOS *ios, ExceptionSink* xsink) {
        return setTerminalAttributesImpl(b, action, ios, xsink);
    }

    //! internal API, must be called before deleting the object if an event queue is set
    DLLLOCAL static void cleanupImpl(QoreFile* b, ExceptionSink* xsink);
    DLLLOCAL void cleanup(ExceptionSink* xsink) {
        cleanupImpl(b, xsink);
    }

    //! makes the file a special file that owns its descriptor (private API)
    /** Like makeSpecial(), it cannot be reopened, but close() and the destruction of the object close the descriptor
        (the ends of File::getPipe()); it can only be called right after the constructor
    */
    DLLLOCAL static void makeOwnedSpecialImpl(QoreFile* b, int fd);
    DLLLOCAL void makeOwnedSpecial(int fd) {
        makeOwnedSpecialImpl(b, fd);
    }

    // NOTE: QoreFile::makeSpecial() can only be called right after the constructor (private API)
    DLLLOCAL static void makeSpecialImpl(QoreFile* b, int sfd);
    DLLLOCAL void makeSpecial(int sfd) {
        makeSpecialImpl(b, sfd);
    }

    //! sets the event queue (not part of the library's pubilc API), must be already referenced before call
    DLLLOCAL static void setEventQueueImpl(QoreFile* b, ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data);
    DLLLOCAL void setEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
        setEventQueueImpl(b, xsink, q, arg, with_data);
    }

};

//! The library-internal operations of QoreFtpClient (qore#4380)
class qore_ftp_client_ops {
public:
    //! the object
    QoreFtpClient* b;

    DLLLOCAL explicit qore_ftp_client_ops(QoreFtpClient* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_ftp_client_ops(H& holder) : qore_ftp_client_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_ftp_client_ops(const QoreFtpClient* n_b) : b(const_cast<QoreFtpClient*>(n_b)) {
    }

    DLLLOCAL explicit qore_ftp_client_ops(QoreFtpClient& r) : b(&r) {
    }

    DLLLOCAL explicit qore_ftp_client_ops(const QoreFtpClient& r) : b(const_cast<QoreFtpClient*>(&r)) {
    }

    //! returns the socket I/O timeout value as an integer in milliseconds
    /** @since Qore 0.8.12.3
    */
    DLLLOCAL static int getTimeoutImpl(QoreFtpClient* b);
    DLLLOCAL int getTimeout() {
        return getTimeoutImpl(b);
    }

    DLLLOCAL static void cleanupImpl(QoreFtpClient* b, ExceptionSink* xsink);
    DLLLOCAL void cleanup(ExceptionSink* xsink) {
        cleanupImpl(b, xsink);
    }

    //! sets the event queue for the control socket
    DLLLOCAL static void setControlEventQueueImpl(QoreFtpClient* b, ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data);
    DLLLOCAL void setControlEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
        setControlEventQueueImpl(b, xsink, q, arg, with_data);
    }

    //! sets the event queue for the data socket
    DLLLOCAL static void setDataEventQueueImpl(QoreFtpClient* b, ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data);
    DLLLOCAL void setDataEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
        setDataEventQueueImpl(b, xsink, q, arg, with_data);
    }

    //! sets the same event queue for data and control sockets
    DLLLOCAL static void setEventQueueImpl(QoreFtpClient* b, ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data);
    DLLLOCAL void setEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
        setEventQueueImpl(b, xsink, q, arg, with_data);
    }

    //! sets the socket I/O timeout value in milliseconds
    /** @since Qore 0.8.12.3
    */
    DLLLOCAL static void setTimeoutImpl(QoreFtpClient* b, int timeout_ms);
    DLLLOCAL void setTimeout(int timeout_ms) {
        setTimeoutImpl(b, timeout_ms);
    }

};

//! The library-internal operations of QoreQueue (qore#4380)
class qore_queue_ops {
public:
    //! the object
    QoreQueue* b;

    DLLLOCAL explicit qore_queue_ops(QoreQueue* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_queue_ops(H& holder) : qore_queue_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_queue_ops(const QoreQueue* n_b) : b(const_cast<QoreQueue*>(n_b)) {
    }

    DLLLOCAL explicit qore_queue_ops(QoreQueue& r) : b(&r) {
    }

    DLLLOCAL explicit qore_queue_ops(const QoreQueue& r) : b(const_cast<QoreQueue*>(&r)) {
    }

    //! Pushes a value at the end of the queue without waiting (internal)
    /** For producers that must not block, such as the I/O thread (issue #5859).

        @param ignore_max true to push even if the queue is full: for a value that ends a stream of values (an end
        or error marker), which must reach the consumer after the values before it

        @note values pushed this way are not counted for the garbage collector's scan, as with pushAndTakeRef():
        only values that cannot be part of a reference cycle (data, plain hashes) may be pushed

        @return see TryPushResult; with QTP_FULL the caller keeps the reference, otherwise the queue takes it

        @since %Qore 3.0
    */
    DLLLOCAL static QoreQueue::TryPushResult tryPushImpl(QoreQueue* b, ExceptionSink* xsink, QoreValue n, bool ignore_max);
    DLLLOCAL QoreQueue::TryPushResult tryPush(ExceptionSink* xsink, QoreValue n, bool ignore_max = false) {
        return tryPushImpl(b, xsink, n, ignore_max);
    }

    //! Returns the queue's item consume hook, which is created if the queue has none (internal)
    /** Shared by the streams whose items are pushed to the queue; see QueueItemConsumeHook (issue #5820).

        @since %Qore 3.0
    */
    DLLLOCAL static std::shared_ptr<QueueItemConsumeHook> getItemConsumeHookImpl(QoreQueue* b);
    DLLLOCAL std::shared_ptr<QueueItemConsumeHook> getItemConsumeHook() {
        return getItemConsumeHookImpl(b);
    }

    //! Adds a listener told when the queue has room after tryPush() found it full, or is deleted or set to an error
    //! state (internal)
    /** @since %Qore 3.0
    */
    DLLLOCAL static void addRoomListenerImpl(QoreQueue* b, std::shared_ptr<QoreQueueRoomListener> listener);
    DLLLOCAL void addRoomListener(std::shared_ptr<QoreQueueRoomListener> listener) {
        addRoomListenerImpl(b, listener);
    }

    //! Removes a room listener (internal)
    /** @since %Qore 3.0
    */
    DLLLOCAL static void removeRoomListenerImpl(QoreQueue* b, const QoreQueueRoomListener* listener);
    DLLLOCAL void removeRoomListener(const QoreQueueRoomListener* listener) {
        removeRoomListenerImpl(b, listener);
    }

    //! Sets the hook that is told about the values taken from the queue, and those dropped with it (internal)
    /** See QoreConsumeHook (issue #5820).

        @since %Qore 3.0
    */
    DLLLOCAL static void setConsumeHookImpl(QoreQueue* b, std::shared_ptr<QoreConsumeHook> hook);
    DLLLOCAL void setConsumeHook(std::shared_ptr<QoreConsumeHook> hook) {
        setConsumeHookImpl(b, hook);
    }

};

#endif // _QORE_INTERN_QOREIOCLASSESOPS_H

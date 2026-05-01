/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_socket_private.h

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

#ifndef _QORE_QORE_SOCKET_PRIVATE_H
#define _QORE_QORE_SOCKET_PRIVATE_H

#include "qore/intern/qore_string_private.h"
#include "qore/AbstractPollState.h"
#include "qore/QoreSocket.h"
#include "qore/InputStream.h"
#include "qore/OutputStream.h"
#include "qore/QoreSandboxManager.h"
#include "qore/QoreStringNode.h"
#include "qore/QoreThreadLock.h"

#include "qore/intern/SSLSocketHelper.h"
#include "qore/intern/QC_Queue.h"

#include "qore/intern/Http2Session.h"
// NOTE: QuicSession.h pulls in ngtcp2, nghttp3, and OpenSSL headers transitively.
// A forward declaration would suffice for the shared_ptr members, but the inline
// methods (addQuicSession, etc.) call QuicSession::getSessionId() and need the
// full definition.  With single-compilation-unit builds this is not a concern.
#include "qore/intern/QuicSession.h"
#include "qore/intern/QoreDatagramDispatcher.h"
#include "qore/intern/qore_thread_intern.h"

#include <cctype>
#include <cerrno>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>
#include <strings.h>
#include <sys/time.h>

#include <ares.h>

#ifdef DARWIN
#include <sys/event.h>
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>

#if defined HAVE_POLL
#include <poll.h>
#elif defined HAVE_SYS_SELECT_H
#include <sys/select.h>
#elif (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
#define HAVE_SELECT 1
#else
#error no async socket I/O APIs available
#endif

#ifndef DEFAULT_SOCKET_BUFSIZE
#define DEFAULT_SOCKET_BUFSIZE (64 * 1024)
#endif

#ifndef QORE_MAX_HEADER_SIZE
#define QORE_MAX_HEADER_SIZE 16384
#endif

#define CHF_HTTP11  (1 << 0)
#define CHF_PROCESS (1 << 1)
#define CHF_REQUEST (1 << 2)

#ifndef DEFAULT_SOCKET_MIN_THRESHOLD_BYTES
#define DEFAULT_SOCKET_MIN_THRESHOLD_BYTES 1024
#endif

static constexpr int SOCK_POLLIN    = (1 << 0);
static constexpr int SOCK_POLLOUT   = (1 << 1);
static constexpr int SOCK_POLLERR   = (1 << 2);
static constexpr int SOCK_POLLTIMER = (1 << 3);

struct qore_socket_private;
class QoreFuture;

DLLLOCAL void concat_target(QoreString& str, const struct sockaddr *addr, const char* type = "target");
DLLLOCAL int qore_socket_exec_close_private(qore_socket_private* priv);
DLLLOCAL int do_read_error(ssize_t rc, const char* method_name, int timeout_ms, ExceptionSink* xsink);
DLLLOCAL int sock_get_raw_error();
DLLLOCAL int sock_get_error();
DLLLOCAL QoreListNode* qore_socket_resolve_addrinfo_asyncio(ExceptionSink* xsink, const char* node,
    const char* service, int family = Q_AF_UNSPEC, int flags = 0, int socktype = Q_SOCK_STREAM,
    int protocol = 0, int timeout_ms = -1);
DLLLOCAL QoreFuture* qore_socket_resolve_addrinfo_asyncio_future(ExceptionSink* xsink, const char* node,
    const char* service, int family = Q_AF_UNSPEC, int flags = 0, int socktype = Q_SOCK_STREAM,
    int protocol = 0, int timeout_ms = -1);
DLLLOCAL QoreFuture* qore_socket_resolve_nameinfo_asyncio_future(ExceptionSink* xsink,
    const struct sockaddr_storage& addr, socklen_t len, int timeout_ms = -1);
DLLLOCAL QoreHashNode* qore_socket_resolve_hostbyaddr_asyncio(ExceptionSink* xsink,
    const struct sockaddr_storage& addr, socklen_t len, int timeout_ms = -1);
DLLLOCAL void qore_socket_error(ExceptionSink* xsink, const char* err, const char* cdesc, const char* mname = nullptr,
    const char* host = nullptr, const char* svc = nullptr, const struct sockaddr *addr = nullptr);
DLLLOCAL void qore_socket_error_intern(int rc, ExceptionSink* xsink, const char* err, const char* cdesc,
    const char* mname = nullptr, const char* host = nullptr, const char* svc = nullptr,
    const struct sockaddr* addr = nullptr);
DLLLOCAL void se_in_op(const char* cname, const char* meth, ExceptionSink* xsink);
DLLLOCAL void se_in_op_thread(const char* cname, const char* meth, ExceptionSink* xsink);
DLLLOCAL void se_not_open(const char* cname, const char* meth, ExceptionSink* xsink, const char* extra = nullptr);
DLLLOCAL void se_timeout(const char* cname, const char* meth, int timeout_ms, ExceptionSink* xsink,
    const char* extra = nullptr);
DLLLOCAL void se_closed(const char* cname, const char* mname, ExceptionSink* xsink);

class Transform;

#ifdef _Q_WINDOWS
#define GETSOCKOPT_ARG_4 char*
#define SETSOCKOPT_ARG_4 const char*
#define SHUTDOWN_ARG SD_BOTH
#define QORE_INVALID_SOCKET ((int)INVALID_SOCKET)
#define QORE_SOCKET_ERROR SOCKET_ERROR
DLLLOCAL int check_windows_rc(int rc);
DLLLOCAL int windows_set_errno();

#ifndef ECONNRESET
#define ECONNRESET WSAECONNRESET
#endif

#else
// UNIX/Cygwin
#define GETSOCKOPT_ARG_4 void*
#define SETSOCKOPT_ARG_4 void*
#define SHUTDOWN_ARG SHUT_RDWR
#define QORE_INVALID_SOCKET -1
#define QORE_SOCKET_ERROR -1
#endif

//! Creates a socket and immediately sets it to non-blocking mode.
/** Returns the socket descriptor, or QORE_INVALID_SOCKET on error (errno set).
*/
DLLLOCAL inline int create_nonblocking_socket(int family, int type, int protocol) {
    int fd = socket(family, type, protocol);
    if (fd == QORE_INVALID_SOCKET) {
        return QORE_INVALID_SOCKET;
    }
#ifdef _Q_WINDOWS
    u_long mode = 1;
    if (ioctlsocket(fd, FIONBIO, &mode) != 0) {
        closesocket(fd);
        return QORE_INVALID_SOCKET;
    }
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        int saved_errno = errno;
        ::close(fd);
        errno = saved_errno;
        return QORE_INVALID_SOCKET;
    }
#endif
    return fd;
}

template <typename T>
class PrivateDataListHolder {
public:
    DLLLOCAL PrivateDataListHolder(ExceptionSink* xsink) : xsink(xsink) {
    }

    DLLLOCAL ~PrivateDataListHolder() {
        for (auto& i : pd_vec)
            i->deref(xsink);
    }

    DLLLOCAL T* add(const QoreObject* o, qore_classid_t cid) {
        T* pd = static_cast<T*>(o->getReferencedPrivateData(cid, xsink));
        if (!pd)
            return nullptr;
        pd_vec.push_back(pd);
        return pd;
    }

private:
    typedef std::vector<T*> pd_vec_t;
    pd_vec_t pd_vec;
    ExceptionSink* xsink;
};

struct qore_socketsource_private {
    QoreStringNode* address;
    QoreStringNode* hostname;

    DLLLOCAL qore_socketsource_private() : address(0), hostname(0) {
    }

    DLLLOCAL ~qore_socketsource_private() {
        if (address)  address->deref();
        if (hostname) hostname->deref();
    }

    DLLLOCAL void setAddress(QoreStringNode* addr) {
        assert(!address);
        address = addr;
    }

    DLLLOCAL void setAddress(const char* addr) {
        assert(!address);
        address = new QoreStringNode(addr);
    }

    DLLLOCAL void setHostName(const char* host) {
        assert(!hostname);
        hostname = new QoreStringNode(host);
    }

    DLLLOCAL void setAll(QoreObject* o, ExceptionSink* xsink) {
        if (address) {
            o->setValue("source", address, xsink);
            address = 0;
        }

        if (hostname) {
            o->setValue("source_host", hostname, xsink);
            hostname = 0;
        }
    }
};

//! Returns true if the calling thread is an async I/O controller thread
/** Used by OptionalNonBlockingHelper to skip unnecessary fd flag toggling
    on the I/O thread where sockets are already non-blocking.
*/
DLLLOCAL bool qore_on_async_io_thread();

#ifdef DEBUG
//! Overrides the async I/O thread flag for focused unit tests
/** @return the previous async I/O thread flag value
*/
DLLLOCAL bool qore_set_async_io_thread_for_test(bool value);
#endif

class OptionalNonBlockingHelper {
public:
    qore_socket_private& sock;
    ExceptionSink* xsink;
    bool set;

    DLLLOCAL OptionalNonBlockingHelper(qore_socket_private& s, bool n_set, ExceptionSink* xs);
    DLLLOCAL ~OptionalNonBlockingHelper();
};

class PrivateQoreSocketTimeoutBase {
public:
    DLLLOCAL PrivateQoreSocketTimeoutBase(qore_socket_private* s) : sock(s), start(sock ? q_clock_getmicros() : 0) {
    }

protected:
    struct qore_socket_private* sock;
    int64 start;
};

class PrivateQoreSocketTimeoutHelper : public PrivateQoreSocketTimeoutBase {
public:
    DLLLOCAL PrivateQoreSocketTimeoutHelper(qore_socket_private* s, const char* op);
    DLLLOCAL ~PrivateQoreSocketTimeoutHelper();

protected:
    const char* op;
};

class PrivateQoreSocketThroughputHelper : public PrivateQoreSocketTimeoutBase {
public:
    DLLLOCAL PrivateQoreSocketThroughputHelper(qore_socket_private* s, bool snd);
    DLLLOCAL ~PrivateQoreSocketThroughputHelper();

    DLLLOCAL void finalize(int64 bytes);

protected:
    bool send;
};

struct qore_socket_private;

struct qore_socket_op_helper {
protected:
    qore_socket_private* s;

public:
    DLLLOCAL qore_socket_op_helper(qore_socket_private* sock);
    DLLLOCAL ~qore_socket_op_helper();
};

class SSLSocketHelperHelper {
protected:
    qore_socket_private* s;
    SSLSocketHelper* ssl;
    bool context_saved = false;

public:
    DLLLOCAL SSLSocketHelperHelper(qore_socket_private* sock, bool set_thread_context = false);

    DLLLOCAL ~SSLSocketHelperHelper();

    DLLLOCAL void error();
};

constexpr int SCIPS_CONNECT = 0;
constexpr int SCIPS_CHECK_CONNECT = 1;

//! Happy Eyeballs (RFC 8305) Connection Attempt Delay in milliseconds
constexpr int HAPPY_EYEBALLS_DELAY_MS = 250;

//! Happy Eyeballs states
constexpr int HEBS_FIRST_CONNECT = 0;
constexpr int HEBS_RACING = 1;
constexpr int HEBS_CONNECTED = 2;
constexpr int HEBS_RESOLVING = 3;

struct SocketResolvedAddrInfo {
    int family = AF_UNSPEC;
    int socktype = SOCK_STREAM;
    int protocol = 0;
    socklen_t addrlen = 0;
    struct sockaddr_storage addr = {};
    std::string canonname;
};

class QoreCaresAddrInfoResolver;

class QoreCaresNameInfoResolver {
public:
    DLLLOCAL QoreCaresNameInfoResolver(const struct sockaddr_storage& addr, socklen_t len, int flags = 0);
    DLLLOCAL ~QoreCaresNameInfoResolver();

    //! Returns 0 when done, 1 when polling must continue, -1 on initialization error
    DLLLOCAL int continuePoll(ExceptionSink* xsink);

    DLLLOCAL const std::string& getHostname() const {
        return hostname;
    }

    DLLLOCAL void getExtraFds(std::vector<std::pair<int, int>>& fds) const;
    DLLLOCAL int getPollTimeoutMs() const;

private:
    DLLLOCAL int start(ExceptionSink* xsink);
    DLLLOCAL void process();
    DLLLOCAL void updateFd(ares_socket_t socket_fd, int readable, int writable);
    DLLLOCAL void complete(int new_status, char* node);

    DLLLOCAL static void callback(void* arg, int status, int, char* node, char*);
    DLLLOCAL static void sockStateCallback(void* arg, ares_socket_t socket_fd, int readable, int writable);

    struct sockaddr_storage addr = {};
    socklen_t len = 0;
    int flags = 0;
    ares_channel_t* channel = nullptr;
    std::unordered_map<int, int> fd_events;
    std::string hostname;
    int status = ARES_SUCCESS;
    bool started = false;
    bool done = false;
};

//! Happy Eyeballs (RFC 8305) connection racing poll state for async I/O
/** Races IPv6 and IPv4 connections with a 250ms stagger.  Manages multiple
    raw file descriptors independently of qore_socket_private; assigns the
    winning fd only after the race concludes.
*/
class SocketConnectInetHappyEyeballsPollState : public AbstractPollState {
public:
    DLLLOCAL SocketConnectInetHappyEyeballsPollState(ExceptionSink* xsink, qore_socket_private* sock, const char* host,
            const char* service, int family = AF_UNSPEC, int type = SOCK_STREAM, int protocol = 0);

    DLLLOCAL ~SocketConnectInetHappyEyeballsPollState();

    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 0 = error (exception raised)
    */
    DLLLOCAL virtual int continuePoll(ExceptionSink* xsink);

    //! Returns extra fds that need to be polled for write readiness (connect completion)
    DLLLOCAL void getExtraFds(std::vector<std::pair<int, int>>& fds) const;

    //! Returns the operation-specific poll timeout, or -1 when no timeout is needed
    DLLLOCAL int getPollTimeoutMs() const;

    //! Returns the primary socket events to poll for the current state
    DLLLOCAL int getPrimaryPollEvents(int rc) const;

    //! Returns true if there are multiple address families to race
    DLLLOCAL bool isRacing() const {
        return multi_family;
    }

    //! Returns the Happy Eyeballs state
    DLLLOCAL int getState() const {
        return he_state;
    }

private:
    struct ConnAttempt {
        int fd = QORE_INVALID_SOCKET;
        size_t addr_idx = 0;
    };

    std::unique_ptr<QoreCaresAddrInfoResolver> resolver;
    qore_socket_private* sock;
    std::string host, service;
    std::vector<SocketResolvedAddrInfo> addrs;
    std::vector<size_t> sorted_addrs;
    std::vector<ConnAttempt> active_attempts;
    size_t next_addr_idx = 0;
    int family = AF_UNSPEC;
    int type = SOCK_STREAM;
    int protocol = 0;
    int prt = -1;
    int he_state = HEBS_RESOLVING;
    int winning_idx = -1;
    bool multi_family = false;

    //! Continue asynchronous DNS resolution
    /** Returns 0 when resolved, 1 when polling must continue, -1 on error */
    DLLLOCAL int continueResolve(ExceptionSink* xsink);

    //! Handles resolver completion and initializes Happy Eyeballs address ordering
    DLLLOCAL int finishResolve(ExceptionSink* xsink);

    //! Start a non-blocking connect to the next address in sorted_addrs
    /** Returns 0 on immediate connect, 1 on EINPROGRESS, -1 on error */
    DLLLOCAL int startNextConnect(ExceptionSink* xsink);

    //! Check if a specific attempt has completed
    /** Returns 0 = connected, 1 = still in progress, -1 = failed */
    DLLLOCAL int checkAttempt(size_t idx);

    //! Close all fds except the winner and assign winner to sock
    DLLLOCAL void assignWinner(ExceptionSink* xsink);

    //! Close all active fds (cleanup)
    DLLLOCAL void closeAllFds();
};

#ifndef _Q_WINDOWS
class SocketConnectUnixPollState : public AbstractPollState {
public:
    DLLLOCAL SocketConnectUnixPollState(ExceptionSink* xsink, qore_socket_private* sock, const char* name,
            int type = SOCK_STREAM, int protocol = 0);

    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 0 = error (exception raised)
    */
    DLLLOCAL virtual int continuePoll(ExceptionSink* xsink);

private:
    qore_socket_private* sock;
    std::string name;
    struct sockaddr_un addr;
    int state = SCIPS_CONNECT;

    DLLLOCAL int doConnect(ExceptionSink* xsink);

    // returns 0 = connected, 1 = try again, -1 = error
    DLLLOCAL int checkConnection(ExceptionSink* xsink);
};
#endif

class SocketConnectSslPollState : public AbstractPollState {
public:
    DLLLOCAL SocketConnectSslPollState(ExceptionSink* xsink, qore_socket_private* sock,
            QoreSSLCertificate* cert = nullptr, QoreSSLPrivateKey* pkey = nullptr);

    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 0 = error (exception raised)
    */
    DLLLOCAL virtual int continuePoll(ExceptionSink* xsink);

private:
    qore_socket_private* sock;

    // returns 0 = connected, 1 = try again, -1 = error
    DLLLOCAL int checkConnection(ExceptionSink* xsink);
};

class SocketAcceptPollState : public AbstractPollState {
public:
    DLLLOCAL SocketAcceptPollState(ExceptionSink* xsink, qore_socket_private* sock, SocketSource* source = nullptr);

    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 0 = error (exception raised)
    */
    DLLLOCAL virtual int continuePoll(ExceptionSink* xsink);

    //! Returns the socket descriptor after a successful call to accept()
    DLLLOCAL int getDescriptor() const {
        return descriptor;
    }

private:
    qore_socket_private* sock;
    SocketSource* source = nullptr;
    int descriptor = -1;
};

class SocketAcceptSslPollState : public AbstractPollState {
public:
    DLLLOCAL SocketAcceptSslPollState(ExceptionSink* xsink, qore_socket_private* sock,
            QoreSSLCertificate* cert = nullptr, QoreSSLPrivateKey* pkey = nullptr);

    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 0 = error (exception raised)
    */
   DLLLOCAL virtual int continuePoll(ExceptionSink* xsink);

private:
    qore_socket_private* sock;
};

class SocketShutdownSslPollState : public AbstractPollState {
public:
    DLLLOCAL SocketShutdownSslPollState(ExceptionSink* xsink, qore_socket_private* sock);

    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 0 = error (exception raised)
    */
    DLLLOCAL virtual int continuePoll(ExceptionSink* xsink);

private:
    qore_socket_private* sock;
};

class SocketSendPollState : public AbstractPollState {
public:
    DLLLOCAL SocketSendPollState(ExceptionSink* xsink, qore_socket_private* sock, const char* data, size_t size);

    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 0 = error (exception raised)
    */
    DLLLOCAL virtual int continuePoll(ExceptionSink* xsink);

    //! Returnd the number of bytes sent so far
    DLLLOCAL size_t getBytesSent() const {
        return sent;
    }

private:
    qore_socket_private* sock;
    const char* data;
    size_t size;
    size_t sent = 0;
};

class SocketRecvPollState : public AbstractPollState {
public:
    DLLLOCAL SocketRecvPollState(ExceptionSink* xsink, qore_socket_private* sock, size_t size);

    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 0 = error (exception raised)
    */
    DLLLOCAL virtual int continuePoll(ExceptionSink* xsink);

    //! Returns the data read
    DLLLOCAL virtual QoreValue takeOutput() {
        QoreValue rv = bin.release();
        bin = nullptr;
        return rv;
    }

    //! Returnd the number of bytes received so far
    DLLLOCAL size_t getBytesReceived() const {
        return received;
    }

private:
    qore_socket_private* sock;
    SimpleRefHolder<BinaryNode> bin;
    size_t size;
    size_t received = 0;
};

class SocketRecvSomePollState : public AbstractPollState {
public:
    DLLLOCAL SocketRecvSomePollState(ExceptionSink* xsink, qore_socket_private* sock, size_t size);

    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 0 = error (exception raised)
    */
    DLLLOCAL virtual int continuePoll(ExceptionSink* xsink);

    //! Returns the data read
    DLLLOCAL virtual QoreValue takeOutput() {
        QoreValue rv = bin.release();
        bin = nullptr;
        return rv;
    }

    //! Returns the number of bytes received
    DLLLOCAL size_t getBytesReceived() const {
        return bin ? bin->size() : 0;
    }

private:
    qore_socket_private* sock;
    SimpleRefHolder<BinaryNode> bin;
    size_t size;
    bool io = false;
};

class SocketRecvPacketPollState : public AbstractPollState {
public:
    DLLLOCAL SocketRecvPacketPollState(ExceptionSink* xsink, qore_socket_private* sock);

    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 0 = error (exception raised)
    */
    DLLLOCAL virtual int continuePoll(ExceptionSink* xsink);

    //! Returns the data read
    DLLLOCAL virtual QoreValue takeOutput() {
        QoreValue rv = bin.release();
        bin = nullptr;
        return rv;
    }

    //! Returnd the number of bytes received so far
    DLLLOCAL size_t getBytesReceived() const {
        return bin ? bin->size() : 0;
    }

private:
    qore_socket_private* sock;
    SimpleRefHolder<BinaryNode> bin;
    bool io = false;
};

class SocketRecvUntilBytesPollState : public AbstractPollState {
public:
    DLLLOCAL SocketRecvUntilBytesPollState(ExceptionSink* xsink, qore_socket_private* sock, const char* bytes,
            size_t size);

    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 0 = error (exception raised)
    */
    DLLLOCAL virtual int continuePoll(ExceptionSink* xsink);

    //! Returns the data read
    DLLLOCAL virtual QoreValue takeOutput() {
        size_t len = bin->size();
        BinaryNode* rv = new BinaryNode(bin->giveBuffer(), len);
        bin = nullptr;
        return rv;
    }

    //! Returnd the number of bytes received so far
    DLLLOCAL size_t getBytesReceived() const {
        return bin ? bin->size() : 0;
    }

private:
    qore_socket_private* sock;
    // we are using QoreStringNode as it has a much better append / concat implementation than BinaryNode
    SimpleRefHolder<QoreStringNode> bin;
    const char* bytes;
    size_t size;
    size_t matched = 0;

    DLLLOCAL int doRecv(ExceptionSink* xsink);
};

//! Non-blocking recvfrom() for UDP datagram sockets
/** Receives a single datagram and captures the source address.
    Returns data as a BinaryNode and source address info as a QoreHashNode
    via takeOutput() (returns a list: [binary data, hash address_info]).

    @since %Qore 2.3
*/
class SocketRecvFromPollState : public AbstractPollState {
public:
    DLLLOCAL SocketRecvFromPollState(ExceptionSink* xsink, qore_socket_private* sock, size_t max_size);

    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 0 = error (exception raised)
    */
    DLLLOCAL virtual int continuePoll(ExceptionSink* xsink);

    //! Returns the received data as a DatagramInfo hash
    DLLLOCAL virtual QoreValue takeOutput();

    //! Returns the number of bytes received
    DLLLOCAL size_t getBytesReceived() const {
        return received;
    }

private:
    qore_socket_private* sock;
    SimpleRefHolder<BinaryNode> bin;
    QoreHashNode* output = nullptr;     //!< Built in continuePoll() with the caller's xsink
    size_t max_size;
    size_t received = 0;
    struct sockaddr_storage src_addr;
    socklen_t src_addr_len = sizeof(struct sockaddr_storage);
    bool io = false;

    //! Build the typed DatagramInfo output hash (requires valid xsink)
    DLLLOCAL void buildOutput(ExceptionSink* xsink);
};

//! Non-blocking sendto() for UDP datagram sockets
/** Sends a datagram to the specified address.

    @since %Qore 2.3
*/
class SocketSendToPollState : public AbstractPollState {
public:
    //! "bin" must be passed already referenced; this class takes ownership of the reference
    DLLLOCAL SocketSendToPollState(ExceptionSink* xsink, qore_socket_private* sock, BinaryNode* bin,
        const struct sockaddr* dest_addr, socklen_t dest_addr_len);

    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 0 = error (exception raised)
    */
    DLLLOCAL virtual int continuePoll(ExceptionSink* xsink);

    //! Returns the number of bytes sent so far
    DLLLOCAL size_t getBytesSent() const {
        return sent;
    }

private:
    qore_socket_private* sock;
    SimpleRefHolder<BinaryNode> bin;    //!< Owns a reference to the data being sent
    const char* data;
    size_t size;
    size_t sent = 0;
    struct sockaddr_storage dest_addr;
    socklen_t dest_addr_len;
};


struct qore_socket_private : public QoreReferenceCounter {
    friend class PrivateQoreSocketTimeoutHelper;
    friend class PrivateQoreSocketThroughputHelper;

    // for client certificate capture
    static thread_local qore_socket_private* current_socket;

    int sock, sfamily, port, stype, sprot;

    // issue #3558: connection sequence to show when a connection has been reestablished
    int64 connection_id = 0;

    const QoreEncoding* enc;

    std::string socketname;
    // issue #3053: client target for SNI
    std::string client_target;
    SSLSocketHelper* ssl = nullptr;
    mutable QoreThreadLock tls_state_cache_m;
    mutable std::string ssl_cipher_name_cache;
    mutable std::string ssl_cipher_version_cache;
    //! Serialises close_internal() SSL shutdown across concurrent callers.
    /** Two paths can reach close_internal() on the same socket without
        holding any shared lock: the I/O thread running an H2 client
        abort → current_op->abort → socket->close, and an app-thread
        disconnect / HTTPClient destructor calling socket->close directly.
        Without serialisation they both read ssl != nullptr, both call
        ssl->shutdown(), and the second touches a freed OpenSSL SSL*
        (backtrace in CI job 170545: EVP_CIPHER_CTX_get0_cipher SIGSEGV
        inside SSL_shutdown on a freed cipher ctx).  Taking this lock only
        around the ssl shutdown/deref block is sufficient — the rest of
        close_internal() is either idempotent or already guarded by its
        own locks. */
    QoreThreadLock ssl_close_m;

    //! One-shot flag for the currently active fd: first @ref prepareForClose
    //! caller wins and performs the fd shutdown + H2 session-closed mark;
    //! subsequent callers in the same close race short-circuit.  Reset only
    //! after a new fd becomes active.
    std::atomic<bool> pre_close_interrupt_fired{false};
    //! ALPN protocols for TLS negotiation (HTTP/2 support)
    std::vector<std::string> alpn_protocols;
    Queue* event_queue = nullptr,   //!< event queue
        * warn_queue = nullptr;     //!< warning queue
    //! protects event queue pointer, callback argument, and data flag lifetime
    mutable QoreThreadLock event_queue_m;
    //! protects warning queue pointer and callback argument lifetime
    mutable QoreThreadLock warning_queue_m;

    // issue #3633: HTTP encoding to assume
    std::string assume_http_encoding = "ISO-8859-1";

    // socket buffer for buffered reads
    char rbuf[DEFAULT_SOCKET_BUFSIZE];

    // content types that imply UTF-8 character encoding
    typedef std::set<std::string> strset_t;
    strset_t utf8_content_type_set = {
        "application/ecmascript",
        "application/json",
        "application/x-javascript",
        "application/javascript",
        "text/javascript", // <- this is the correct MIME type for JavaScript
        "application/ld+json",
        "application/yaml", // <- this is the correct MIME type for YAML
        "application/x-yaml",
        "text/yaml",
    };

    // current buffer size
    size_t buflen = 0,
        bufoffset = 0;

    std::atomic<int64> tl_warning_us{0};     // timeout threshold for network action warning in microseconds
    std::atomic<double> tp_warning_bs{0.0};  // throughput warning threshold in B/s
    std::atomic<int64> tp_bytes_sent{0};  // throughput: bytes sent
    std::atomic<int64> tp_bytes_recv{0};  // throughput: bytes received
    std::atomic<int64> tp_us_sent{0};     // throughput: time sending
    std::atomic<int64> tp_us_recv{0};     // throughput: time receiving
    std::atomic<int64> tp_us_min{0};         // throughput: minimum time for transfer to be considered

    //! callback argument for the warning queue
    QoreValue warn_callback_arg{};
    //! argument for the event queue
    QoreValue event_arg{};
    bool del = false,
        http_exp_chunked_body = false,
        ssl_accept_all_certs = false,
        ssl_capture_remote_cert = false,
        event_data = false,
        sse_got_cr = false;
    int in_op = -1,
        ssl_verify_mode = SSL_VERIFY_NONE;

    //! Configured TCP_USER_TIMEOUT (ms); 0 = unset, applied at connect.
    /** Set via QoreSocket::setUserTimeout.  Re-applied at every successful
        TCP connect inside confirmConnected() so it survives disconnect/
        reconnect cycles.  Has no effect on UNIX-domain or UDP sockets.
        On platforms without TCP_USER_TIMEOUT (BSD/macOS/Windows) the
        value is stored but no kernel option is set.
    */
    int tcp_user_timeout_ms = 0;

    //! Back-pointer to the outer my_socket_priv's mutex — used by sync I/O
    //! helpers to release the lock during their poll wait phase.
    /** Wired by my_socket_priv's constructor when this qore_socket_private
        is owned by a my_socket_priv.  Null for bare QoreSocket instances
        that are not wrapped in the object layer.  When null, sync helpers
        fall back to holding the (non-existent) lock — no contamination
        concern because there is no outer lock to contend for.

        @since %Qore 2.3
    */
    QoreThreadLock* outer_lock = nullptr;

    //! Generation counter bumped on every close / fd swap.
    /** Async controller-backed socket operations snapshot and re-verify this
        counter across readiness waits: if it has changed, the fd they were
        operating on has been replaced (via close(), migration, etc.) and any
        captured socket state is stale.  The correct response is to abort the
        operation with SOCKET-CLOSED.

        Mirrors @ref SocketPollOperationBase::getFdGeneration() at the
        socket level rather than the poll-op level.

        @since %Qore 2.3
    */
    uint32_t fd_generation = 0;

    //! Directional ownership for controller-backed raw socket I/O sections.
    /** Bare QoreSocket instances do not have the QoreSocketObject mutex and
        non-blocking flags, but synchronous raw socket methods now delegate to
        the async I/O controller as well.  This per-direction owner prevents two
        callers from interleaving controller-backed receive or send operations on
        the same raw socket while still allowing nested helper calls from the
        owning thread.

        @since %Qore 2.3
    */
    mutable QoreThreadLock async_sequence_m;
    int async_sequence_owner_tid[3] = {-1, -1, -1};
    int async_sequence_count[3] = {0, 0, 0};

#ifdef DEBUG
    //! Debug-only: when true, the next controller wait simulates an fd swap.
    /** Tests set this flag via the debug Socket API to exercise the
        fd_generation re-verification path in AsyncIoController execution.
        The flag is a one-shot: the controller bumps @ref fd_generation and
        clears the flag before entering its wait, so a single invocation
        produces exactly one simulated swap.

        @note DEBUG builds only.  The hook is unconditionally ignored
        in release builds because the member does not exist there.

        @since %Qore 2.3
    */
    bool debug_force_fd_swap_next_wait = false;
#endif

    // issue #3512: the remote certificate captured
    QoreObject* remote_cert = nullptr;

    // issue #3818: verbose certificate verification error info
    QoreStringNode* ssl_err_str = nullptr;

    //! HTTP/2 session for persistent HTTP/2 connections
    /** The session is created during startPollReadHttp2Request() and reused
        for subsequent operations on the same connection.
        Uses shared_ptr for thread-safe atomic reference counting.
    */
    Http2SessionPtr h2_session;

    //! Active HTTP/2 stream ID for WebSocket or other bidirectional streams
    /** When set to a positive value, send() and recv() operations will use
        HTTP/2 DATA frames on this stream instead of raw socket I/O.
        Set to -1 when no stream is active.
    */
    int32_t h2_active_stream_id = -1;
    mutable QoreThreadLock h2_active_stream_lock;
    mutable QoreThreadLock h2_session_lock;
    std::unordered_map<int, int32_t> h2_active_stream_ids;

    //! Flag to prevent recursion in HTTP/2 receive path
    /** When true, socket data-availability checks bypass active stream handling.
        This is set when receiving HTTP/2 protocol frames to avoid infinite recursion.
    */
    bool h2_receiving_frames = false;

    //! Maximum size for chunked HTTP body reads (0 = unlimited)
    /** When set, controller-backed chunked body readers will
        raise an HTTP-BODY-TOO-LARGE exception if the accumulated body exceeds
        this limit.
    */
    std::atomic<int64> max_chunked_body_size{0};

    //! Maximum request body size for HTTP/2 streams (0 = unlimited)
    /** Propagated to Http2Session when created; DATA frame accumulation exceeding
        this limit causes the stream to be reset with REFUSED_STREAM.
    */
    std::atomic<int64> max_http2_body_size{0};

    //! Whether to advertise ENABLE_CONNECT_PROTOCOL in HTTP/2 server SETTINGS
    /** When false, the server does not advertise extended CONNECT protocol support
        (RFC 8441), so clients will not attempt WebSocket over HTTP/2 CONNECT.
        Defaults to true.
    */
    bool h2_enable_connect_protocol = true;

    //! Per-stream callbacks for HTTP/2 client multiplexing
    /** Maps stream_id -> QoreValue callback for response dispatch.
        Used by Http2ClientMultiplexPollOperation to route completed responses
        to the appropriate handler.
    */
    std::unordered_map<int32_t, QoreValue> h2_stream_callbacks;
    mutable QoreThreadLock h2_stream_callbacks_lock;

    //! Stream completion callback for HTTP/2 client multiplexing
    /** Called when a stream completes (response received or error).
        Set by Http2ClientMultiplexPollOperation for response routing.
    */
    std::function<void(int32_t, Http2StreamInfo*, ExceptionSink*)> h2_stream_complete_callback;

    //! Connection-ID based datagram dispatcher for QUIC multi-connection support
    /** Routes incoming UDP datagrams to the correct QuicSession based on DCID.
        Used by SocketQuicServerPollOperation for CID-based packet routing.
        Lazily initialized to avoid overhead for non-QUIC sockets.
    */
    std::unique_ptr<QoreDatagramDispatcher> quic_dispatcher;

    //! Get or create the QUIC datagram dispatcher (lazy initialization)
    /** Thread-safe: uses quic_sessions_lock to protect the lazy init.
        Although typically called from the I/O thread (which holds sock->priv->m),
        this lock makes it safe regardless of caller context.

        @note The returned reference remains valid for the lifetime of the socket
        because quic_dispatcher is only set once (lazy init) and never cleared
        or moved.
    */
    DLLLOCAL QoreDatagramDispatcher& getQuicDispatcher() {
        AutoLocker al(quic_sessions_lock);
        if (!quic_dispatcher) {
            quic_dispatcher = std::make_unique<QoreDatagramDispatcher>();
        }
        return *quic_dispatcher;
    }

    //! Authoritative map of session_id -> QuicSession for multi-connection support.
    /** Multiple QUIC clients can connect to a single UDP socket.
        Each session is identified by a unique session_id.
        Protected by quic_sessions_lock; SocketQuicServerPollOperation::sessions_
        is a single-threaded working copy used only from the I/O thread.
    */
    std::unordered_map<int64_t, std::shared_ptr<QuicSession>> quic_sessions;
    //! Lock ordering: QoreSocketObject::priv->m → quic_sessions_lock → QuicSession::mtx_ (never reverse)
    mutable QoreThreadLock quic_sessions_lock;

    //! Add a QUIC session to the session map
    DLLLOCAL void addQuicSession(const std::shared_ptr<QuicSession>& session) {
        AutoLocker al(quic_sessions_lock);
        quic_sessions[session->getSessionId()] = session;
    }

    //! Remove a QUIC session from the session map
    DLLLOCAL void removeQuicSession(int64_t session_id) {
        AutoLocker al(quic_sessions_lock);
        quic_sessions.erase(session_id);
    }

    //! Get a QUIC session by session ID
    /** Acquires quic_sessions_lock internally; caller may hold priv->m (safe per
        lock ordering: priv->m → quic_sessions_lock → QuicSession::mtx_).
    */
    DLLLOCAL std::shared_ptr<QuicSession> getQuicSession(int64_t session_id) {
        AutoLocker al(quic_sessions_lock);
        auto it = quic_sessions.find(session_id);
        return it != quic_sessions.end() ? it->second : nullptr;
    }

    //! Get the first QUIC session ID (for client connections with a single session)
    /** @return session ID or 0 if no sessions exist (0 is never a valid session
        ID since IDs start at 1)
        @note Returns an arbitrary session from the unordered_map; used only for
        single-session client sockets where exactly one session exists.
    */
    DLLLOCAL int64_t getFirstQuicSessionId(ExceptionSink* xsink = nullptr) const {
        AutoLocker al(quic_sessions_lock);
        if (quic_sessions.empty()) {
            return 0;
        }
        if (quic_sessions.size() != 1) {
            if (xsink) {
                xsink->raiseException("QUIC-SESSION-ERROR",
                    "getFirstQuicSessionId() is only valid for single-session client sockets; "
                    "this socket has %d sessions", (int)quic_sessions.size());
            }
            return 0;
        }
        return quic_sessions.begin()->first;
    }

    //! Shared server SSL_CTX for QUIC sessions (for session ticket key continuity)
    /** Session tickets are encrypted with the SSL_CTX's ticket key, so all
        sessions for a listener must share one SSL_CTX. This ensures that a
        ticket issued by one session can be decrypted by another, enabling
        QUIC 0-RTT (early data).

        Lazily initialized by getOrCreateQuicServerSslCtx(). Reference-counted
        via SSL_CTX_up_ref()/SSL_CTX_free(); QuicSession holds a reference,
        and this pointer holds the "master" reference. Cleaned up in destructor
        and close_internal() via freeQuicServerSslCtx().
    */
    SSL_CTX* quic_server_ssl_ctx_ = nullptr;
    std::mutex quic_server_ssl_ctx_lock_;  //!< protects lazy init of quic_server_ssl_ctx_

    //! Get or create the shared server SSL_CTX for QUIC
    /** Thread-safe: uses quic_server_ssl_ctx_lock_ internally. Creates the SSL_CTX
        once with TLS 1.3, cert/key, ALPN callback, max_early_data=0xffffffff
        (RFC 9001 §4.6.1), and 2 session tickets.
        @param cert X.509 certificate
        @param pk private key
        @param xsink exception sink
        @return SSL_CTX pointer (owned by this socket), or nullptr on error
    */
    DLLLOCAL SSL_CTX* getOrCreateQuicServerSslCtx(QoreSSLCertificate* cert, QoreSSLPrivateKey* pk,
                                                   ExceptionSink* xsink);

    //! Free the shared server SSL_CTX if allocated (thread-safe)
    DLLLOCAL void freeQuicServerSslCtx() {
        std::lock_guard<std::mutex> lock(quic_server_ssl_ctx_lock_);
        if (quic_server_ssl_ctx_) {
            SSL_CTX_free(quic_server_ssl_ctx_);
            quic_server_ssl_ctx_ = nullptr;
        }
    }

#ifdef DARWIN
    //! Write end of a notification pipe used by Socket::poll() on macOS
    /** When a socket FD is closed during a kqueue poll, macOS silently removes
        the kqueue filter without delivering an event. Writing to this pipe wakes
        up the kevent() call so it can detect the closed socket.
        -1 when no poll is active.
    */
    std::atomic<int> poll_notify_fd{-1};
#endif

    DLLLOCAL qore_socket_private(int n_sock = QORE_INVALID_SOCKET, int n_sfamily = AF_UNSPEC,
            int n_stype = SOCK_STREAM, int n_prot = 0, const QoreEncoding* n_enc = QCS_DEFAULT) :
            sock(n_sock), sfamily(n_sfamily), port(-1), stype(n_stype), sprot(n_prot), enc(n_enc) {
    }

    DLLLOCAL ~qore_socket_private() {
        // Clear dispatcher references in all QUIC sessions before destroying the dispatcher
        {
            AutoLocker al(quic_sessions_lock);
            for (auto& [id, session] : quic_sessions) {
                session->clearDispatcher();
            }
        }

        close_internal();

        // Free the shared server SSL_CTX after all sessions are destroyed
        // (sessions hold their own SSL_CTX_up_ref'd references via SSL_new)
        freeQuicServerSslCtx();

        // must be dereferenced and removed before deleting
        assert(!event_queue);
        assert(!warn_queue);
    }

    DLLLOCAL void ref() const {
        ROreference();
    }

    DLLLOCAL void deref() {
        if (ROdereference()) {
            delete this;
        }
    }

    DLLLOCAL bool isOpen() {
        return sock != QORE_INVALID_SOCKET;
    }

    //! Throws an exception if another thread owns controller-backed I/O in @a direction
    DLLLOCAL int checkAsyncSequenceAllowedForTid(ExceptionSink* xsink, unsigned direction, int tid) const;

    //! Starts a directional controller-backed I/O section for the current thread
    DLLLOCAL int startAsyncSequenceIo(ExceptionSink* xsink, unsigned direction);

    //! Clears a directional controller-backed I/O section for the current thread
    DLLLOCAL void clearAsyncSequenceIo(unsigned direction);

    DLLLOCAL int32_t getH2ActiveStreamId() const {
        bool use_thread_map;
        {
            AutoLocker al(h2_session_lock);
            use_thread_map = !h2_session || h2_session->isServer();
        }
        if (use_thread_map) {
            AutoLocker al(h2_active_stream_lock);
            auto it = h2_active_stream_ids.find(q_gettid());
            return it == h2_active_stream_ids.end() ? -1 : it->second;
        }
        return h2_active_stream_id;
    }

    DLLLOCAL int32_t getH2ActiveThreadStreamId() const {
        AutoLocker al(h2_active_stream_lock);
        auto it = h2_active_stream_ids.find(q_gettid());
        return it == h2_active_stream_ids.end() ? -1 : it->second;
    }

    DLLLOCAL void setH2ActiveStreamId(int32_t stream_id) {
        bool use_thread_map;
        {
            AutoLocker al(h2_session_lock);
            use_thread_map = !h2_session || h2_session->isServer();
        }
        if (use_thread_map) {
            AutoLocker al(h2_active_stream_lock);
            int tid = q_gettid();
            if (stream_id > 0) {
                h2_active_stream_ids[tid] = stream_id;
            } else {
                h2_active_stream_ids.erase(tid);
            }
        } else {
            h2_active_stream_id = stream_id;
        }
    }

    //! Registers a callback for HTTP/2 stream completion (client multiplexing)
    /** @param stream_id the stream ID to register
        @param callback the callback value (typically a code reference)
    */
    DLLLOCAL void registerH2StreamCallback(int32_t stream_id, const QoreValue& callback) {
        AutoLocker al(h2_stream_callbacks_lock);
        h2_stream_callbacks[stream_id] = callback.refSelf();
    }

    //! Unregisters a callback for HTTP/2 stream completion
    /** @param stream_id the stream ID to unregister
        @param xsink exception sink for dereferencing the callback
    */
    DLLLOCAL void unregisterH2StreamCallback(int32_t stream_id, ExceptionSink* xsink) {
        QoreValue callback;
        {
            AutoLocker al(h2_stream_callbacks_lock);
            auto it = h2_stream_callbacks.find(stream_id);
            if (it != h2_stream_callbacks.end()) {
                callback = it->second;
                h2_stream_callbacks.erase(it);
            }
        }
        if (callback) {
            callback.discard(xsink);
        }
    }

    //! Gets and removes a callback for HTTP/2 stream completion
    /** @param stream_id the stream ID
        @return the callback value (caller takes ownership), or nothing if not found
    */
    DLLLOCAL QoreValue takeH2StreamCallback(int32_t stream_id) {
        AutoLocker al(h2_stream_callbacks_lock);
        auto it = h2_stream_callbacks.find(stream_id);
        if (it != h2_stream_callbacks.end()) {
            QoreValue callback = it->second;
            h2_stream_callbacks.erase(it);
            return callback;
        }
        return QoreValue();
    }

    //! Sets the stream completion callback for HTTP/2 client multiplexing
    DLLLOCAL void setH2StreamCompleteCallback(
            std::function<void(int32_t, Http2StreamInfo*, ExceptionSink*)> callback) {
        h2_stream_complete_callback = std::move(callback);
    }

    //! Clears the stream completion callback
    DLLLOCAL void clearH2StreamCompleteCallback() {
        h2_stream_complete_callback = nullptr;
    }

    //! Returns true if ALPN protocols have been configured
    DLLLOCAL bool hasAlpnProtocols() const {
        return !alpn_protocols.empty();
    }

    DLLLOCAL int close() {
        return close_internal();
    }

    DLLLOCAL int shutdown_direct() {
        if (h2_session) {
            h2_session->markClosed();
        }
        return sock != QORE_INVALID_SOCKET ? ::shutdown(sock, SHUTDOWN_ARG) : 0;
    }

    //! Interrupts any in-flight sync I/O on this socket without taking the
    //! outer mutex.
    /** Must be called by the outer @c QoreSocketObject::close() BEFORE it
        acquires @c priv->m.  Without this pre-step, a deadlock arises
        when another thread is holding @c priv->m (or @c Http2Session::m
        via an H2 sync poll / send) and is itself blocked in a @c ::poll
        / @c SSL_read / @c SSL_write on this fd — that thread cannot
        release its mutex until the I/O returns, which in turn requires
        the fd to be closed or shut down.  The close path is the caller
        responsible for that shutdown, but it's stuck waiting for the
        same mutex.  See @c grpc-shutdown-deadlock.md for the full
        scenario (Socket::close vs an H2 session lock held by an in-flight
        poll / send).

        Two actions, both safe to perform concurrently with any other
        thread's in-flight I/O:

          1. Mark the @ref Http2Session (if any) as closed via
             @ref Http2Session::markClosed.  Loops in
             @c receiveData and stream-completion waits check
             @ref Http2Session::isSessionClosed and return promptly —
             releasing @c Http2Session::m.

          2. Issue @c ::shutdown(fd, SHUT_RDWR) on the raw fd.  The
             kernel returns any pending blocking @c recv / @c send with
             EPIPE / ECONNRESET, making @c isSocketDataAvailable /
             @c asyncIoWait return.  The fd is read without any lock —
             the race with a concurrent @c close_and_reset is harmless
             because @c close_and_reset runs inside the very close path
             we're about to continue from (AutoLocker on priv->m in
             @c QoreSocketObject::close), so no fd reuse can have
             happened yet by the same thread.

        Idempotent: @ref pre_close_interrupt_fired ensures at most one
        thread actually issues the shutdown/mark-closed side effects even
        if multiple concurrent @c close() calls arrive.

        @since %Qore 2.3
    */
    DLLLOCAL void prepareForClose() {
        bool expected = false;
        if (!pre_close_interrupt_fired.compare_exchange_strong(
                expected, true,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            return;  // another concurrent close already did the interrupt
        }
        // Wake H2 sync consumers (receiveData's retry loops and
        // isStreamComplete).
        if (h2_session) {
            h2_session->markClosed();
        }
        // Shut down the raw fd to unblock any ::poll / SSL_read /
        // SSL_write currently in flight on this socket.  A concurrent
        // close_and_reset() on the SAME thread that called us would be
        // a re-entrant close and is guarded by the outer AutoLocker on
        // priv->m; cross-thread, the fd here either (a) is still open
        // and we unblock the poll, or (b) has already been closed by a
        // racing close, in which case ::shutdown returns EBADF silently.
        int fd = sock;
        if (fd != QORE_INVALID_SOCKET) {
            ::shutdown(fd, SHUTDOWN_ARG);
        }
    }

    DLLLOCAL int close_and_reset() {
        assert(sock != QORE_INVALID_SOCKET);
        int rc;
        while (true) {
#ifdef _Q_WINDOWS
            rc = ::closesocket(sock);
#else
            rc = ::close(sock);
#endif
            // try again if close was interrupted by a signal
            if (!rc || sock_get_error() != EINTR) {
                break;
            }
        }
        //printd(5, "qore_socket_private::close_and_reset(this: %p) close(%d) returned %d\n", this, sock, rc);
        sock = QORE_INVALID_SOCKET;
        // Bump fd_generation so any sync I/O helper currently in its
        // poll-wait phase (with outer_lock released) sees the generation
        // change on re-acquire and returns QSE_NOT_OPEN instead of
        // retrying on a dead fd.
        ++fd_generation;
        if (buflen) {
            buflen = 0;
        }
        if (bufoffset) {
            bufoffset = 0;
        }
        if (del) {
            del = false;
        }
        if (port != -1) {
            port = -1;
        }
        if (in_op >= 0) {
            in_op = -1;
        }
        if (http_exp_chunked_body) {
            http_exp_chunked_body = false;
        }
        if (sse_got_cr) {
            sse_got_cr = false;
        }
        sfamily = AF_UNSPEC;
        stype = SOCK_STREAM;
        sprot = 0;
        // issue #3053: clear hostname for SNI
        client_target.clear();
        return rc;
    }

    DLLLOCAL int close_internal() {
        //printd(5, "qore_socket_private::close_internal(this: %p) sock: %d\n", this, sock);
        if (ssl_err_str) {
            ssl_err_str->deref();
            ssl_err_str = nullptr;
        }
        if (remote_cert) {
            remote_cert->deref(nullptr);
            remote_cert = nullptr;
        }
        // Reset shared_ptr - will delete session if this is the last reference
        h2_session.reset();
        // Clear dispatcher references before clearing sessions to avoid dangling pointers
        {
            AutoLocker al(quic_sessions_lock);
            for (auto& [id, session] : quic_sessions) {
                session->clearDispatcher();
            }
            quic_sessions.clear();
        }
        // Free shared server SSL_CTX after all sessions are released
        freeQuicServerSslCtx();
        // Clear HTTP/2 client multiplexing state
        {
            AutoLocker al(h2_stream_callbacks_lock);
            for (auto& it : h2_stream_callbacks) {
                it.second.discard(nullptr);
            }
            h2_stream_callbacks.clear();
        }
        h2_stream_complete_callback = nullptr;
        if (sock >= 0) {
            // Cancel any in-flight I/O on this socket BEFORE touching the SSL
            // state.  The SSL object is not thread-safe (see SSLSocketHelper.h
            // line 70: "all operations must be already locked"), and a concurrent
            // SSL_read/SSL_write on another thread colliding with the SSL_shutdown
            // below corrupts the internal cipher context — SIGSEGV inside
            // EVP_CIPHER_get_mode / EVP_CIPHER_CTX_get0_cipher has been observed
            // under H2 teardown load (e.g. CI jobs 170545, 170659).
            //
            // A TCP-level shutdown(SHUT_RDWR) makes any pending/blocking SSL I/O
            // on other threads return a fatal error (EPIPE/ECONNRESET/WANT_READ
            // loops resolve to failure), so by the time SSL_shutdown runs below
            // no other thread is still inside an SSL call on this context.  This
            // is the "cancel I/O first, then shutdown" invariant.
            //
            // Ignore errors from the shutdown syscall: already-shutdown sockets
            // return ENOTCONN, and we only need best-effort cancellation here.
            ::shutdown(sock, SHUTDOWN_ARG);

            // if an SSL connection has been established, shut it down first.
            // Serialise the shutdown/deref pair so concurrent close callers
            // cannot both see ssl != nullptr and both invoke shutdown on
            // the same OpenSSL SSL* (one side derefs it to 0 → SSL_free,
            // the other side crashes inside SSL_shutdown on freed state —
            // see ssl_close_m comment).
            {
                AutoLocker al(ssl_close_m);
                if (ssl) {
                    ssl->shutdown();
                    ssl->deref();
                    ssl = nullptr;
                }
            }

            if (!socketname.empty()) {
                if (del) {
                    unlink(socketname.c_str());
                }
                socketname.clear();
            }
            do_close_event();
            // issue #3558: increment the connection sequence here. so the connection sequence is different as soon as
            // it's closed
            ++connection_id;

            int rc = close_and_reset();

#ifdef DARWIN
            // Signal any active kqueue poll that this socket was closed;
            // on macOS, closing a monitored FD silently removes its kqueue
            // filter without delivering an event.
            // Use exchange to atomically claim and clear the fd, ensuring
            // only one thread writes to the pipe for this socket.
            int nfd = poll_notify_fd.exchange(-1, std::memory_order_acq_rel);
            if (nfd >= 0) {
                char c = 1;
                while (::write(nfd, &c, 1) == -1 && errno == EINTR) {}
            }
#endif

            return rc;
        } else {
            return 0;
        }
    }

    DLLLOCAL void setAssumedEncoding(const char* str) {
        assume_http_encoding = str;
    }

    DLLLOCAL const char* getAssumedEncoding() const {
        return assume_http_encoding.c_str();
    }

    DLLLOCAL int getSendTimeout() const {
        struct timeval tv;

#if defined(HPUX) && defined(__ia64) && defined(__LP64__)
        // on HPUX 64-bit the OS defines socklen_t to be 8 bytes
        // but the library expects a 32-bit value
        int size = sizeof(struct timeval);
#else
        socklen_t size = sizeof(struct timeval);
#endif

        if (getsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (GETSOCKOPT_ARG_4)&tv, (socklen_t *)&size))
            return -1;

        return tv.tv_sec * 1000 + tv.tv_usec / 1000;
    }

    DLLLOCAL int getRecvTimeout() const {
        struct timeval tv;

#if defined(HPUX) && defined(__ia64) && defined(__LP64__)
        // on HPUX 64-bit the OS defines socklen_t to be 8 bytes
        // but the library expects a 32-bit value
        int size = sizeof(struct timeval);
#else
        socklen_t size = sizeof(struct timeval);
#endif

        if (getsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (GETSOCKOPT_ARG_4)&tv, (socklen_t *)&size))
            return -1;

        return tv.tv_sec * 1000 + tv.tv_usec / 1000;
    }

    DLLLOCAL int getPort() {
        // if we don't need to find out what port we are, then return current value
        if (sock == QORE_INVALID_SOCKET || (sfamily != AF_INET && sfamily != AF_INET6) || port > 0)
            return port;

        // otherwise find out what port we're connected to
        struct sockaddr_storage addr;
#if defined(HPUX) && defined(__ia64) && defined(__LP64__)
        // on HPUX 64-bit the OS defines socklen_t to be 8 bytes, but the library expects a 32-bit value
        int size = sizeof addr;
#else
        socklen_t size = sizeof addr;
#endif

        if (getsockname(sock, (struct sockaddr *)&addr, (socklen_t *)&size) < 0)
            return -1;

        port = q_get_port_from_addr((const struct sockaddr *)&addr);
        return port;
    }

    DLLLOCAL static void do_header(const char* key, QoreString& hdr, const QoreValue& v) {
        switch (v.getType()) {
            case NT_STRING:
                hdr.sprintf("%s: %s\r\n", key, v.get<const QoreStringNode>()->c_str());
                break;
            case NT_INT:
                hdr.sprintf("%s: " QLLD "\r\n", key, v.getAsBigInt());
                break;
            case NT_FLOAT: {
                hdr.sprintf("%s: ", key);
                size_t offset = hdr.size();
                hdr.sprintf("%f\r\n", v.getAsFloat());
                // issue 1556: external modules that call setlocale() can change
                // the decimal point character used here from '.' to ','
                // only search the double added, QoreString::sprintf() concatenates
                q_fix_decimal(&hdr, offset);
                break;
            }
            case NT_NUMBER:
                hdr.sprintf("%s: ", key);
                v.get<const QoreNumberNode>()->toString(hdr);
                hdr.concat("\r\n");
                break;
            case NT_BOOLEAN:
                hdr.sprintf("%s: %d\r\n", key, (int)v.getAsBool());
                break;
        }
    }

    // issue #3879: must add Content-Length in responses if not present, even if there is no message body
    /** see https://www.w3.org/Protocols/rfc2616/rfc2616-sec9.html
    */
    DLLLOCAL static void do_headers(QoreString& hdr, const QoreHashNode* headers, size_t size, bool addsize = true,
            bool add_chunked = false) {
        // RFC-2616 4.4 (http://tools.ietf.org/html/rfc2616#section-4.4)
        // add Content-Length: 0 to headers for responses without a body where there is no transfer-encoding
        if (headers) {
            ConstHashIterator hi(headers);

            while (hi.next()) {
                const QoreValue v = hi.get();
                const char* key = hi.getKey();
                if (!size && (addsize || add_chunked)) {
                    if (!strcasecmp(key, "transfer-encoding")) {
                        addsize = false;
                        add_chunked = false;
                    } else if (!strcasecmp(key, "content-type")
                        && (v.getType() == NT_STRING)
                        && (*v.get<const QoreStringNode>() == "text/event-stream")) {
                        addsize = false;
                    }
                }
                if ((addsize || size) && !strcasecmp(key, "content-length")) {
                    // ignore Content-Length given manually
                    continue;
                }
                if (v.getType() == NT_LIST) {
                    ConstListIterator li(v.get<const QoreListNode>());
                    while (li.next())
                        do_header(key, hdr, li.getValue());
                } else
                    do_header(key, hdr, v);
            }
        }
        // add data and content-length header if necessary
        if (size || addsize) {
            hdr.sprintf("Content-Length: %zu\r\n", size);
            //printd(5, "qore_socket_private::do_headers() added Content-Length: %zu\n", size);
        }
        // add Transfer-Encoding: chunked for send_callback/input_stream responses (HTTP/1.x only)
        if (add_chunked) {
            hdr.concat("Transfer-Encoding: chunked\r\n");
        }

        hdr.concat("\r\n");
    }

    DLLLOCAL int listen(int backlog = 20) {
        if (sock == QORE_INVALID_SOCKET)
            return QSE_NOT_OPEN;
        if (in_op >= 0)
            return QSE_IN_OP;
#ifdef _Q_WINDOWS
        if (::listen(sock, backlog)) {
            // set errno
            sock_get_error();
            return -1;
        }
        return 0;
#else
        return ::listen(sock, backlog);
#endif
    }

    DLLLOCAL int accept_intern(ExceptionSink* xsink, struct sockaddr *addr, socklen_t *size) {
        assert(xsink);

        OptionalNonBlockingHelper onbh(*this, true, xsink);
        if (*xsink) {
            return -1;
        }

        while (true) {
            int rc = ::accept(sock, addr, size);
            if (rc != QORE_INVALID_SOCKET) {
                // The listener is non-blocking while owned by the async I/O controller.
                // Some platforms propagate that flag to accepted sockets; return accepted
                // descriptors in blocking mode so legacy callers and SSL setup keep their
                // expected socket state.
#ifdef _Q_WINDOWS
                u_long mode = 0;
                ioctlsocket(rc, FIONBIO, &mode);
#else
                int arg = fcntl(rc, F_GETFL, 0);
                if (arg >= 0 && (arg & O_NONBLOCK)) {
                    fcntl(rc, F_SETFL, arg & ~O_NONBLOCK);
                }
#endif
                return rc;
            }

            int err = sock_get_error();
            if (err == EINTR) {
                continue;
            }
            if (err == EAGAIN
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
                    || err == EWOULDBLOCK
#endif
                    ) {
                return QSE_TIMEOUT;
            }

            qore_socket_error(xsink, "SOCKET-ACCEPT-ERROR", "error in accept()", 0, 0, 0, addr);
            return -1;
        }
    }

    // returns a new socket
    DLLLOCAL int accept_internal(ExceptionSink* xsink, SocketSource* source) {
        assert(xsink);
        if (sock == QORE_INVALID_SOCKET) {
            xsink->raiseException("SOCKET-NOT-OPEN", "socket must be opened, bound, and in a listening state before "
                "new connections can be accepted");
            return QSE_NOT_OPEN;
        }
        if (in_op >= 0) {
            if (in_op == q_gettid()) {
                se_in_op("Socket", "accept", xsink);
                return QSE_IN_OP;
            }
            se_in_op_thread("Socket", "accept", xsink);
            return QSE_IN_OP_THREAD;
        }
        int rc;
        if (sfamily == AF_UNIX) {
#ifdef _Q_WINDOWS
            xsink->raiseException("SOCKET-ACCEPT-ERROR", "UNIX sockets are not available under Windows");
            return -1;
#else
            struct sockaddr_un addr_un;

#if defined(HPUX) && defined(__ia64) && defined(__LP64__)
            // on HPUX 64-bit the OS defines socklen_t to be 8 bytes
            // but the library expects a 32-bit value
            int size = sizeof(struct sockaddr_un);
#else
            socklen_t size = sizeof(struct sockaddr_un);
#endif
            rc = accept_intern(xsink, (struct sockaddr *)&addr_un, (socklen_t *)&size);
            //printd(1, "qore_socket_private::accept_internal() " QSD " bytes returned\n", size);

            if (rc >= 0 && source) {
                QoreStringNode* addr = new QoreStringNode(enc);
                addr->sprintf("UNIX socket: %s", socketname.c_str());
                source->priv->setAddress(addr);
                source->priv->setHostName("localhost");
            }
#endif // windows
        } else if (sfamily == AF_INET || sfamily == AF_INET6) {
            struct sockaddr_storage addr_in;
#if defined(HPUX) && defined(__ia64) && defined(__LP64__)
            // on HPUX 64-bit the OS defines socklen_t to be 8 bytes
            // but the library expects a 32-bit value
            int size = sizeof(addr_in);
#else
            socklen_t size = sizeof(addr_in);
#endif

            rc = accept_intern(xsink, (struct sockaddr *)&addr_in, (socklen_t *)&size);
            //printd(1, "qore_socket_private::accept_internal() rc: %d, %d bytes returned\n", rc, size);

            if (rc >= 0 && source) {
                // get ipv4 or ipv6 address
                char ifname[INET6_ADDRSTRLEN];
                if (inet_ntop(addr_in.ss_family, qore_get_in_addr((struct sockaddr *)&addr_in), ifname,
                    sizeof(ifname))) {
                    source->priv->setHostName(ifname);
                    source->priv->setAddress(ifname);
                }
            }
        } else {
            // should not happen
            xsink->raiseException("SOCKET-ACCEPT-ERROR", "do not know how to accept connections with address "
                "family %d", sfamily);
            rc = -1;
        }
        return rc;
    }

    DLLLOCAL QoreHashNode* getEvent(int event, int source = QORE_SOURCE_SOCKET) const {
        QoreHashNode* h = new QoreHashNode(autoTypeInfo);
        if (event_arg) {
            h->setKeyValue("arg", event_arg.refSelf(), nullptr);
        }

        h->setKeyValue("event", event, nullptr);
        h->setKeyValue("source", source, nullptr);
        h->setKeyValue("id", (int64)this, nullptr);

        return h;
    }

    DLLLOCAL void cleanupQueues(ExceptionSink* xsink) {
        Queue* old_event_queue = nullptr;
        QoreValue old_event_arg;
        {
            AutoLocker al(event_queue_m);
            if (event_queue) {
                event_queue->pushAndTakeRef(getEvent(QORE_EVENT_DELETED));

                old_event_queue = event_queue;
                old_event_arg = event_arg;
                event_queue = nullptr;
                event_arg = QoreValue();
                event_data = false;
            }
        }
        if (old_event_arg) {
            old_event_arg.discard(xsink);
        }
        if (old_event_queue) {
            old_event_queue->deref(xsink);
        }
        clearWarningQueue(xsink);
    }

    DLLLOCAL void setEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
        Queue* old_queue = nullptr;
        QoreValue old_arg;
        {
            AutoLocker al(event_queue_m);
            old_queue = event_queue;
            old_arg = event_arg;
            event_queue = q;
            event_arg = arg;
            event_data = with_data;
        }
        if (old_arg) {
            old_arg.discard(xsink);
        }
        if (old_queue) {
            old_queue->deref(xsink);
        }
    }

    DLLLOCAL bool hasEventQueue() const {
        AutoLocker al(event_queue_m);
        return event_queue;
    }

    DLLLOCAL bool isEventDataEnabled() const {
        AutoLocker al(event_queue_m);
        return event_queue && event_data;
    }

    DLLLOCAL Queue* getEventQueue() const {
        AutoLocker al(event_queue_m);
        return event_queue;
    }

    DLLLOCAL void swapEventQueueState(qore_socket_private& s) {
        if (&s == this) {
            return;
        }

        qore_socket_private* first;
        qore_socket_private* second;
        if (std::less<qore_socket_private*>()(this, &s)) {
            first = this;
            second = &s;
        } else {
            first = &s;
            second = this;
        }

        AutoLocker al(first->event_queue_m);
        AutoLocker bl(second->event_queue_m);

        std::swap(event_queue, s.event_queue);
        std::swap(event_arg, s.event_arg);
        std::swap(event_data, s.event_data);
    }

    DLLLOCAL void do_start_ssl_event() {
        AutoLocker al(event_queue_m);
        if (event_queue) {
            event_queue->pushAndTakeRef(getEvent(QORE_EVENT_START_SSL));
        }
    }

    DLLLOCAL void do_ssl_established_event() {
        AutoLocker al(event_queue_m);
        if (event_queue) {
            QoreHashNode* h = getEvent(QORE_EVENT_SSL_ESTABLISHED);
            h->setKeyValue("cipher", new QoreStringNode(ssl->getCipherName()), nullptr);
            h->setKeyValue("cipher_version", new QoreStringNode(ssl->getCipherVersion()), nullptr);
            event_queue->pushAndTakeRef(h);
        }
    }

    DLLLOCAL void do_connect_event(int af, const struct sockaddr* addr, const char* target,
            const char* service = nullptr, int prt = -1) {
        AutoLocker al(event_queue_m);
        if (event_queue) {
            QoreHashNode* h = getEvent(QORE_EVENT_CONNECTING);
            QoreStringNode* str = q_addr_to_string2(addr);
            if (str) {
                h->setKeyValue("address", str, nullptr);
            } else {
                h->setKeyValue("error", q_strerror(sock_get_error()), nullptr);
            }
            q_af_to_hash(af, *h, nullptr);
            h->setKeyValue("target", new QoreStringNode(target), nullptr);
            if (service)
                h->setKeyValue("service", new QoreStringNode(service), nullptr);
            if (prt != -1)
                h->setKeyValue("port", prt, nullptr);
            event_queue->pushAndTakeRef(h);
        }
    }

    DLLLOCAL void do_connected_event() {
        AutoLocker al(event_queue_m);
        if (event_queue) {
            event_queue->pushAndTakeRef(getEvent(QORE_EVENT_CONNECTED));
        }
    }

    DLLLOCAL void do_data_event_intern(int event, int source, const QoreStringNode& str) const {
        AutoLocker al(event_queue_m);
        if (event_queue && event_data && str.size()) {
            ReferenceHolder<QoreHashNode> h(getEvent(event, source), nullptr);
            h->setKeyValue("data", str.refSelf(), nullptr);
            event_queue->pushAndTakeRef(h.release());
        }
    }

    DLLLOCAL void do_data_event(int event, int source, const QoreStringNode& str) const {
        do_data_event_intern(event, source, str);
    }

    DLLLOCAL void do_data_event(int event, int source, const BinaryNode& b) const {
        AutoLocker al(event_queue_m);
        if (event_queue && event_data && b.size()) {
            ReferenceHolder<QoreHashNode> h(getEvent(event, source), nullptr);
            h->setKeyValue("data", b.refSelf(), nullptr);
            event_queue->pushAndTakeRef(h.release());
        }
    }

    DLLLOCAL void do_data_event(int event, int source, const void* data, size_t size) const {
        AutoLocker al(event_queue_m);
        if (event_queue && event_data && size) {
            ReferenceHolder<QoreHashNode> h(getEvent(event, source), nullptr);
            SimpleRefHolder<BinaryNode> b(new BinaryNode);
            b->append(data, size);
            h->setKeyValue("data", b.release(), nullptr);
            event_queue->pushAndTakeRef(h.release());
        }
    }

    DLLLOCAL void do_header_event(int event, int source, const QoreHashNode& hdr) const {
        AutoLocker al(event_queue_m);
        if (event_queue && event_data && !hdr.empty()) {
            ReferenceHolder<QoreHashNode> h(getEvent(event, source), nullptr);
            h->setKeyValue("headers", hdr.refSelf(), nullptr);
            event_queue->pushAndTakeRef(h.release());
        }
    }

    DLLLOCAL void do_chunked_read(int event, size_t bytes, size_t total_read, int source) {
        AutoLocker al(event_queue_m);
        if (event_queue) {
            QoreHashNode* h = getEvent(event, source);
            if (event == QORE_EVENT_HTTP_CHUNKED_DATA_RECEIVED)
                h->setKeyValue("read", bytes, nullptr);
            else
                h->setKeyValue("size", bytes, nullptr);
            h->setKeyValue("total_read", total_read, nullptr);
            event_queue->pushAndTakeRef(h);
        }
    }

    DLLLOCAL void do_read_http_header(int event, const QoreHashNode* headers, int source) {
        AutoLocker al(event_queue_m);
        if (event_queue) {
            QoreHashNode* h = getEvent(event, source);
            h->setKeyValue("headers", headers->hashRefSelf(), nullptr);
            event_queue->pushAndTakeRef(h);
        }
    }

    DLLLOCAL void do_send_http_message_event(const QoreString& str, const QoreHashNode* headers, int source) {
        AutoLocker al(event_queue_m);
        if (event_queue) {
            QoreHashNode* h = getEvent(QORE_EVENT_HTTP_SEND_MESSAGE, source);
            h->setKeyValue("message", new QoreStringNode(str), nullptr);
            //printd(5, "do_send_http_message_event() str='%s' headers: %p (%d %s)\n", str.c_str(), headers, headers->getType(), headers->getTypeName());
            h->setKeyValue("headers", headers->copy(), nullptr);
            event_queue->pushAndTakeRef(h);
        }
    }

    DLLLOCAL void do_close_event() {
        AutoLocker al(event_queue_m);
        if (event_queue) {
            event_queue->pushAndTakeRef(getEvent(QORE_EVENT_CHANNEL_CLOSED));
        }
    }

    DLLLOCAL void do_read_event(size_t bytes_read, size_t total_read, size_t bufsize = 0, int source = QORE_SOURCE_SOCKET) {
        // post bytes read on event queue, if any
        AutoLocker al(event_queue_m);
        if (event_queue) {
            QoreHashNode* h = getEvent(QORE_EVENT_PACKET_READ, source);
            h->setKeyValue("read", bytes_read, nullptr);
            h->setKeyValue("total_read", total_read, nullptr);
            // set total bytes to read and remaining bytes if bufsize > 0
            if (bufsize > 0)
                h->setKeyValue("total_to_read", bufsize, nullptr);
            event_queue->pushAndTakeRef(h);
        }
    }

    DLLLOCAL void do_send_event(int bytes_sent, int total_sent, int bufsize) {
        // post bytes sent on event queue, if any
        AutoLocker al(event_queue_m);
        if (event_queue) {
            QoreHashNode* h = getEvent(QORE_EVENT_PACKET_SENT);
            h->setKeyValue("sent", bytes_sent, nullptr);
            h->setKeyValue("total_sent", total_sent, nullptr);
            h->setKeyValue("total_to_send", bufsize, nullptr);
            event_queue->pushAndTakeRef(h);
        }
    }

    DLLLOCAL void do_resolve_event(const char* host, const char* service = 0) {
        // post bytes sent on event queue, if any
        AutoLocker al(event_queue_m);
        if (event_queue) {
            QoreHashNode* h = getEvent(QORE_EVENT_HOSTNAME_LOOKUP);
            if (host)
                h->setKeyValue("name", new QoreStringNode(host), nullptr);
            if (service)
                h->setKeyValue("service", new QoreStringNode(service), nullptr);
            event_queue->pushAndTakeRef(h);
        }
    }

    DLLLOCAL void do_resolved_event(const struct sockaddr* addr) {
        // post bytes sent on event queue, if any
        AutoLocker al(event_queue_m);
        if (event_queue) {
            QoreHashNode* h = getEvent(QORE_EVENT_HOSTNAME_RESOLVED);
            QoreStringNode* str = q_addr_to_string2(addr);
            if (str)
                h->setKeyValue("address", str, nullptr);
            else
                h->setKeyValue("error", q_strerror(sock_get_error()), nullptr);
            int prt = q_get_port_from_addr(addr);
            if (prt > 0)
                h->setKeyValue("port", prt, nullptr);
            q_af_to_hash(addr->sa_family, *h, nullptr);
            event_queue->pushAndTakeRef(h);
        }
    }

    DLLLOCAL void do_content_length_event(size_t len, int source) {
        AutoLocker al(event_queue_m);
        if (event_queue) {
            QoreHashNode* h = getEvent(QORE_EVENT_HTTP_CONTENT_LENGTH, source);
            h->setKeyValue("len", len, nullptr);
            event_queue->pushAndTakeRef(h);
        }
    }

    DLLLOCAL void do_redirect_event(const QoreStringNode* loc, const QoreStringNode* msg, int source) {
        AutoLocker al(event_queue_m);
        if (event_queue) {
            QoreHashNode* h = getEvent(QORE_EVENT_HTTP_REDIRECT, source);
            if (loc) {
                h->setKeyValue("location", loc->refSelf(), nullptr);
            }
            if (msg) {
                h->setKeyValue("status_message", msg->refSelf(), nullptr);
            }
            event_queue->pushAndTakeRef(h);
        }
    }

    DLLLOCAL int64 getObjectIDForEvents() const {
        return (int64)this;
    }

    DLLLOCAL bool peekSslApplicationData(const char* mname, ExceptionSink* xsink) {
        assert(xsink);
        assert(ssl);

        OptionalNonBlockingHelper nbh(*this, true, xsink);
        if (*xsink) {
            return false;
        }

        size_t real_io = 0;
        int rc = ssl->doNonBlockingIo(xsink, mname, rbuf, 1, PEEK, real_io);
        if (*xsink) {
            return false;
        }
        return !rc && real_io > 0;
    }

    DLLLOCAL int close_and_exit() {
        if (sock != QORE_INVALID_SOCKET)
            close_and_reset();
        return -1;
    }

    DLLLOCAL void resetCloseInterrupt() {
        pre_close_interrupt_fired.store(false, std::memory_order_release);
    }

    DLLLOCAL void confirmConnected(const char* host) {
        resetCloseInterrupt();
        do_connected_event();

        // issue #3053: save hostname for SNI
        if (host) {
            client_target = host;
        }

        // Re-apply configured TCP_USER_TIMEOUT on the new fd if set.  Only
        // meaningful for TCP (skips UNIX-domain and UDP).
#ifdef TCP_USER_TIMEOUT
        if (tcp_user_timeout_ms > 0 && sock != QORE_INVALID_SOCKET
                && (sfamily == AF_INET || sfamily == AF_INET6)
                && stype == SOCK_STREAM) {
            unsigned int v = (unsigned int)tcp_user_timeout_ms;
            (void)setsockopt(sock, IPPROTO_TCP, TCP_USER_TIMEOUT,
                (SETSOCKOPT_ARG_4)&v, sizeof(v));
        }
#endif
    }

    DLLLOCAL int sock_errno_err(const char* err, const char* desc, ExceptionSink* xsink) {
        //sock = QORE_INVALID_SOCKET;
        qore_socket_error(xsink, err, desc);
        return -1;
    }

    DLLLOCAL int set_non_blocking(bool non_blocking, ExceptionSink* xsink) {
        assert(xsink);
        // ignore call when socket already closed (e.g. during shutdown cleanup)
        if (sock == QORE_INVALID_SOCKET) {
            return -1;
        }

#ifdef _Q_WINDOWS
        u_long mode = non_blocking ? 1 : 0;
        int rc = ioctlsocket(sock, FIONBIO, &mode);
        if (check_windows_rc(rc)) {
            return sock_errno_err("SOCKET-CONNECT-ERROR", "error in ioctlsocket(FIONBIO)", xsink);
        }
#else
        int arg;

        // get socket descriptor status flags
        if ((arg = fcntl(sock, F_GETFL, 0)) < 0) {
            return sock_errno_err("SOCKET-CONNECT-ERROR", "error in fcntl() getting socket descriptor status "
                "flag", xsink);
        }

        if (non_blocking) { // set non-blocking
            arg |= O_NONBLOCK;
        } else { // set blocking
            arg &= ~O_NONBLOCK;
        }

        if (fcntl(sock, F_SETFL, arg) < 0) {
            return sock_errno_err("SOCKET-CONNECT-ERROR", "error in fcntl() setting socket descriptor status "
                "flag", xsink);
        }
#endif
        //printd(5, "qore_socket_private::set_non_blocking() set: %d\n", non_blocking);

        return 0;
    }

    //! Sort addresses into interleaved IPv6/IPv4 order per RFC 8305
    DLLLOCAL static void sortAddressesHappyEyeballs(struct addrinfo* aip, std::vector<struct addrinfo*>& sorted,
            bool& multi_family) {
        std::vector<struct addrinfo*> v6, v4, other;
        for (struct addrinfo* p = aip; p; p = p->ai_next) {
            if (p->ai_family == AF_INET6) {
                v6.push_back(p);
            } else if (p->ai_family == AF_INET) {
                v4.push_back(p);
            } else {
                other.push_back(p);
            }
        }

        multi_family = !v6.empty() && !v4.empty();

        // Interleave: v6 first, then v4, then any others
        size_t i6 = 0, i4 = 0;
        while (i6 < v6.size() || i4 < v4.size()) {
            if (i6 < v6.size()) {
                sorted.push_back(v6[i6++]);
            }
            if (i4 < v4.size()) {
                sorted.push_back(v4[i4++]);
            }
        }
        for (auto* p : other) {
            sorted.push_back(p);
        }
    }

    // returns 0 = success, -1 = error
    DLLLOCAL int openUNIX(int sock_type = SOCK_STREAM, int protocol = 0) {
        if (sock != QORE_INVALID_SOCKET)
            close();

        assert(sock == QORE_INVALID_SOCKET);
        if ((sock = socket(AF_UNIX, sock_type, protocol)) == QORE_INVALID_SOCKET) {
            return -1;
        }
        resetCloseInterrupt();

        sfamily = AF_UNIX;
        stype = sock_type;
        sprot = protocol;
        port = -1;
        return 0;
    }

    // returns 0 = success, -1 = error
    DLLLOCAL int openINET(int family = AF_INET, int sock_type = SOCK_STREAM, int protocol = 0) {
        if (sock != QORE_INVALID_SOCKET)
            close();

        assert(sock == QORE_INVALID_SOCKET);
        if ((sock = socket(family, sock_type, protocol)) == QORE_INVALID_SOCKET)
            return -1;
        resetCloseInterrupt();

        sfamily = family;
        stype = sock_type;
        sprot = protocol;
        port = -1;
        return 0;
    }

    DLLLOCAL int reuse(int opt) {
        //printf("qore_socket_private::reuse(%s)\n", opt ? "true" : "false");
        return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (SETSOCKOPT_ARG_4)&opt, sizeof(int));
    }

    // the only place where xsink is optional
    DLLLOCAL int bindIntern(struct sockaddr* ai_addr, size_t ai_addrlen, int prt, bool reuseaddr, ExceptionSink* xsink = 0) {
        // Check sandbox network security restrictions for bind
        QoreSandboxManagerHelper smh;
        if (smh && xsink) {
            int proto = (stype == SOCK_STREAM) ? QSEC_NET_TCP :
                        (stype == SOCK_DGRAM) ? QSEC_NET_UDP :
                        (ai_addr->sa_family == AF_UNIX) ? QSEC_NET_UNIX : QSEC_NET_ALL;
            if (!smh->network().checkBind(ai_addr, ai_addrlen, proto, xsink)) {
                close();
                return -1;
            }
        }

        reuse(reuseaddr);

        if ((::bind(sock, ai_addr, ai_addrlen)) == QORE_SOCKET_ERROR) {
            if (xsink)
                qore_socket_error(xsink, "SOCKET-BIND-ERROR", "error in bind()", 0, 0, 0, ai_addr);
            close();
            return -1;
        }

        // set port number
        if (prt)
            port = prt;
        else {
            // get port number
#if defined(HPUX) && defined(__ia64) && defined(__LP64__)
            // on HPUX 64-bit the OS defines socklen_t to be 8 bytes, but the library expects a 32-bit value
            int len = ai_addrlen;
#else
            socklen_t len = ai_addrlen;
#endif

            if (getsockname(sock, ai_addr, &len))
                port = -1;
            else
                port = q_get_port_from_addr(ai_addr);
        }
        return 0;
    }

    // bind to UNIX domain socket file
    DLLLOCAL int bindUNIX(ExceptionSink* xsink, const char* name, int socktype = SOCK_STREAM, int protocol = 0) {
        assert(xsink);
#ifdef _Q_WINDOWS
        xsink->raiseException("SOCKET-BINDUNIX-ERROR", "UNIX sockets are not available under Windows");
        return -1;
#else
        close();

        // try to open socket if necessary
        if (openUNIX(socktype, protocol)) {
            xsink->raiseErrnoException("SOCKET-BIND-ERROR", errno, "error opening UNIX socket ('%s') for bind", name);
            return -1;
        }

        struct sockaddr_un addr;
        addr.sun_family = AF_UNIX;
        // copy path and terminate if necessary
        strncpy(addr.sun_path, name, sizeof(addr.sun_path) - 1);
        addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

        if (bindIntern((sockaddr*)&addr, sizeof(struct sockaddr_un), -1, false, xsink))
            return -1;

        // save socket file name for deleting on close
        socketname = addr.sun_path;
        // delete UNIX domain socket on close
        del = true;
        return 0;
#endif // windows
    }

    DLLLOCAL int getPeerSockAddr(ExceptionSink* xsink, struct sockaddr_storage& addr, socklen_t& len) const {
        assert(xsink);
        if (sock == QORE_INVALID_SOCKET) {
            se_not_open("Socket", "getPeerInfo", xsink);
            return -1;
        }

        len = sizeof addr;
        if (getpeername(sock, (struct sockaddr*)&addr, &len)) {
            qore_socket_error(xsink, "SOCKET-GETPEERINFO-ERROR", "error in getpeername()");
            return -1;
        }

        return 0;
    }

    DLLLOCAL int getSocketSockAddr(ExceptionSink* xsink, struct sockaddr_storage& addr, socklen_t& len) const {
        assert(xsink);
        if (sock == QORE_INVALID_SOCKET) {
            se_not_open("Socket", "getSocketInfo", xsink);
            return -1;
        }

#if defined(HPUX) && defined(__ia64) && defined(__LP64__)
        // on HPUX 64-bit the OS defines socklen_t to be 8 bytes, but the library expects a 32-bit value
        int local_len = sizeof addr;
#else
        socklen_t local_len = sizeof addr;
#endif

        if (getsockname(sock, (struct sockaddr*)&addr, &local_len)) {
            qore_socket_error(xsink, "SOCKET-GETSOCKETINFO-ERROR", "error in getsockname()");
            return -1;
        }

        len = local_len;
        return 0;
    }

    // only called from qore-bound code - always with xsink
    DLLLOCAL QoreHashNode* getPeerInfo(ExceptionSink* xsink, bool host_lookup = true) const {
        struct sockaddr_storage addr;
        socklen_t len;
        if (getPeerSockAddr(xsink, addr, len)) {
            return 0;
        }
        return getAddrInfo(addr, len, host_lookup);
    }

    // only called from qore-bound code - always with xsink
    DLLLOCAL QoreHashNode* getSocketInfo(ExceptionSink* xsink, bool host_lookup = true) const {
        struct sockaddr_storage addr;
        socklen_t len;
        if (getSocketSockAddr(xsink, addr, len)) {
            return 0;
        }
        return getAddrInfo(addr, len, host_lookup);
    }

    DLLLOCAL QoreHashNode* getAddrInfo(const struct sockaddr_storage& addr, socklen_t len,
            bool host_lookup = true) const {
        return getAddrInfo(addr, len, host_lookup, socketname);
    }

    DLLLOCAL static QoreHashNode* getAddrInfo(const struct sockaddr_storage& addr, socklen_t len, bool host_lookup,
            const std::string& socketname) {
        const char* hostname = nullptr;
        char host[INET6_ADDRSTRLEN];
        if ((addr.ss_family == AF_INET || addr.ss_family == AF_INET6) && host_lookup
                && inet_ntop(addr.ss_family, qore_get_in_addr((struct sockaddr*)&addr), host, sizeof(host))) {
            hostname = host;
        }
        return makeAddrInfo(addr, len, socketname, hostname);
    }

    DLLLOCAL static QoreHashNode* makeAddrInfo(const struct sockaddr_storage& addr, socklen_t len,
            const std::string& socketname, const char* hostname = nullptr) {
        QoreHashNode* h = new QoreHashNode(autoTypeInfo);

        if (addr.ss_family == AF_INET || addr.ss_family == AF_INET6) {
            if (hostname && *hostname) {
                QoreStringNode* hoststr = new QoreStringNode(hostname);
                h->setKeyValue("hostname", hoststr, 0);
                h->setKeyValue("hostname_desc", QoreAddrInfo::getAddressDesc(addr.ss_family, hoststr->c_str()), 0);
            }

            // get ipv4 or ipv6 address
            char ifname[INET6_ADDRSTRLEN];
            if (inet_ntop(addr.ss_family, qore_get_in_addr((struct sockaddr*)&addr), ifname, sizeof(ifname))) {
                QoreStringNode* addrstr = new QoreStringNode(ifname);
                h->setKeyValue("address", addrstr, 0);
                h->setKeyValue("address_desc", QoreAddrInfo::getAddressDesc(addr.ss_family, addrstr->c_str()), 0);
            }

            int tport;
            if (addr.ss_family == AF_INET) {
                struct sockaddr_in* s = (struct sockaddr_in*)&addr;
                tport = ntohs(s->sin_port);
            } else {
                struct sockaddr_in6* s = (struct sockaddr_in6*)&addr;
                tport = ntohs(s->sin6_port);
            }

            h->setKeyValue("port", tport, 0);
        }
#ifndef _Q_WINDOWS
        else if (addr.ss_family == AF_UNIX) {
            QoreStringNode* addrstr;
            if (!socketname.empty()) {
                addrstr = new QoreStringNode(socketname);
            } else {
                // for accepted connections, get the address from the sockaddr_un structure
                struct sockaddr_un* sun = (struct sockaddr_un*)&addr;
                if (len > offsetof(struct sockaddr_un, sun_path) && sun->sun_path[0]) {
                    addrstr = new QoreStringNode(sun->sun_path);
                } else {
                    addrstr = new QoreStringNode("<anonymous unix socket>");
                }
            }
            h->setKeyValue("address", addrstr, 0);
            h->setKeyValue("address_desc", QoreAddrInfo::getAddressDesc(addr.ss_family, addrstr->c_str()), 0);
        }
#endif

        h->setKeyValue("family", addr.ss_family, 0);
        h->setKeyValue("familystr", new QoreStringNode(QoreAddrInfo::getFamilyName(addr.ss_family)), 0);

        return h;
    }

    // set backwards-compatible object members on accept
    // to be (hopefully) deleted in a future version of qore
    DLLLOCAL static void setAccept(QoreObject* o, const QoreHashNode& info) {
        QoreValue family = info.getKeyValue("family");
        QoreValue address = info.getKeyValue("address");
        if (address.getType() != NT_STRING) {
            return;
        }

        int64 family_id = family.getType() == NT_INT ? family.getAsBigInt() : -1;
        if (family_id == AF_INET || family_id == AF_INET6) {
            o->setValue("source", address.refSelf(), 0);

            QoreValue hostname = info.getKeyValue("hostname");
            if (hostname.getType() == NT_STRING) {
                o->setValue("source_host", hostname.refSelf(), 0);
            }
        }
#ifndef _Q_WINDOWS
        else if (family_id == AF_UNIX) {
            QoreStringNode* astr = new QoreStringNode;
            astr->sprintf("UNIX socket: %s", address.get<const QoreStringNode>()->c_str());
            o->setValue("source", astr, 0);
            o->setValue("source_host", new QoreStringNode("localhost"), 0);
        }
#endif
    }

    //! read one byte from buffer
    /**
        @param output output char

        @return -1 = no data in buffer, 0 = OK
    */
    DLLLOCAL int readByteFromBuffer(char& output) {
        // must be checked if open/connected before this function is called
        assert(sock != QORE_INVALID_SOCKET);

        // always returned buffered data first
        if (!buflen) {
            return -1;
        }

        output = *(rbuf + bufoffset);
        if (buflen == 1) {
            buflen = 0;
            bufoffset = 0;
        } else {
            --buflen;
            ++bufoffset;
        }
        return 0;
    }

    //! processes a header string to a hash and raises socket events
    DLLLOCAL QoreHashNode* processHttpHeaderString(ExceptionSink* xsink, QoreStringNodeHolder& hdr,
            QoreHashNode* info, int source, const char* headers_raw_key = "headers-raw") {
        const char* buf = hdr->c_str();
        char* p;
        if ((p = (char*)strstr(buf, "\r\n"))) {
            *p = '\0';
            p += 2;
        } else if ((p = (char*)strchr(buf, '\n'))) {
            *p = '\0';
            ++p;
        } else if ((p = (char*)strchr(buf, '\r'))) {
            *p = '\0';
            ++p;
        } else {
            // Header readers only pass strings that satisfy one of the above
            // conditions; an embedded 0 can still make these searches invalid.
            xsink->raiseException("SOCKET-HTTP-ERROR", "invalid header received with embedded nulls in "
                "Socket::readHTTPHeader()");
            return nullptr;
        }

        char* t1;
        if (!(t1 = (char*)strstr(buf, "HTTP/"))) {
            xsink->raiseExceptionArg("SOCKET-HTTP-ERROR", hdr.release(), "missing HTTP version string in "
                "first header line in Socket::readHTTPHeader()");
            return nullptr;
        }

        ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);

        // process header flags
        int flags = CHF_PROCESS;

        // get version
        {
            QoreStringNode* hv = new QoreStringNode(t1 + 5, 3, enc);
            h->setKeyValue("http_version", hv, nullptr);
            if (*hv == "1.1") {
                flags |= CHF_HTTP11;
            }
        }

        // if we are getting a response
        // key for info if applicable
        const char* info_key;
        if (t1 == buf) {
            char* t2 = (char*)strchr(buf + 8, ' ');
            if (t2) {
                t2++;
                if (isdigit(*(t2))) {
                    h->setKeyValue("status_code", atoi(t2), nullptr);
                    if (strlen(t2) > 4) {
                        h->setKeyValue("status_message", new QoreStringNode(t2 + 4), nullptr);
                    }
                }
            }
            // write the status line as the "response-uri" key in the info hash if present
            // NOTE: this is not a URI, so the name is not really appropriate
            info_key = "response-uri";
        } else { // get method and path
            char* t2 = (char*)strchr(buf, ' ');
            if (t2) {
                *t2 = '\0';
                h->setKeyValue("method", new QoreStringNode(buf), nullptr);
                t2++;
                t1 = strchr(t2, ' ');
                if (t1) {
                    *t1 = '\0';
                    //printd(5, "found path '%s'\n", t2);
                    // the path is returned as-is with no decodings - use decode_url() to decode
                    h->setKeyValue("path", new QoreStringNode(t2, enc), nullptr);
                }
            }
            info_key = "request-uri";
            flags |= CHF_REQUEST;
        }

        // write status line or request line to the info hash and raise a data event if applicable
        bool emit_data_event = isEventDataEnabled();
        if (info || emit_data_event) {
            QoreStringNode* status_line = new QoreStringNode(buf);
            if (emit_data_event) {
                do_data_event_intern(QORE_EVENT_SOCKET_DATA_READ, source, *status_line);
            }
            if (info) {
                info->setKeyValue(info_key, status_line, nullptr);
            } else {
                status_line->deref(nullptr);
            }
        }

        bool close = convertHeaderToHash(*h, p, flags, info, &http_exp_chunked_body, headers_raw_key);
        do_read_http_header(QORE_EVENT_HTTP_MESSAGE_RECEIVED, *h, source);

        // process header info
        if ((flags & CHF_REQUEST) && info) {
            info->setKeyValue("close", close, 0);
        }

        return h.release();
    }

    // info must be already referenced for the assignment, if present
    DLLLOCAL int runHeaderCallback(ExceptionSink* xsink, const char* cname, const char* mname,
            const ResolvedCallReferenceNode& callback, QoreThreadLock* l, const QoreHashNode* hdr, QoreHashNode* info,
            bool send_aborted = false, QoreObject* obj = nullptr) {
        assert(xsink);
        assert(obj);
        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
        QoreHashNode* arg = new QoreHashNode(autoTypeInfo);
        arg->setKeyValue("hdr", hdr ? hdr->refSelf() : nullptr, xsink);
        arg->setKeyValue("info", info, xsink);
        if (obj)
            arg->setKeyValue("obj", obj->refSelf(), xsink);
        arg->setKeyValue("send_aborted", send_aborted, xsink);
        args->push(arg, nullptr);

        ValueHolder rv(xsink);
        return runCallback(xsink, cname, mname, rv, callback, l, *args);
    }

    DLLLOCAL int runTrailerCallback(ExceptionSink* xsink, const char* cname, const char* mname,
            const ResolvedCallReferenceNode& callback, QoreThreadLock* l, ReferenceHolder<QoreHashNode>& hdr) {
        ValueHolder rv(xsink);
        if (runCallback(xsink, cname, mname, rv, callback, l, nullptr))
            return -1;

        switch (rv->getType()) {
            case NT_NOTHING:
                break;
            case NT_HASH: {
                hdr = rv.release().get<QoreHashNode>();
                break;
            }
            default:
                xsink->raiseException("HTTP-TRAILER-ERROR", "chunked callback returned type '%s'; expecting 'hash' "
                    "or 'NOTHING'", rv->getTypeName());
                return -1;
        }
        return 0;
    }

    DLLLOCAL int runDataCallback(ExceptionSink* xsink, const char* cname, const char* mname,
            const ResolvedCallReferenceNode& callback, QoreThreadLock* l, const AbstractQoreNode* data, bool chunked) {
        assert(xsink);
        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
        QoreHashNode* arg = new QoreHashNode(autoTypeInfo);
        arg->setKeyValue("data", data->realCopy(), xsink);
        arg->setKeyValue("chunked", chunked, xsink);
        args->push(arg, nullptr);

        ValueHolder rv(xsink);
        return runCallback(xsink, cname, mname, rv, callback, l, *args);
    }

    DLLLOCAL int runCallback(ExceptionSink* xsink, const char* cname, const char* mname, ValueHolder& res,
            const ResolvedCallReferenceNode& callback, QoreThreadLock* l, const QoreListNode* args = nullptr) {
        assert(xsink);
        // Callback time is included in socket performance measurement.

        // unlock and execute callback
        {
            AutoUnlocker al(l);
            res = callback.execValue(args, xsink);
        }

        // check exception and socket status
        assert(xsink);
        return *xsink ? -1 : 0;
    }

    DLLLOCAL void getSendHttpMessageHeaders(QoreString& hdr, QoreHashNode* info, const char* method, const char* path,
            const char* http_version, const QoreHashNode* headers, size_t size, int source) {
        // prepare header string
        hdr.sprintf("%s %s HTTP/%s", method, path && path[0] ? path : "/", http_version);

        // write request-uri key if info hash is non-null
        if (info) {
            info->setKeyValue("request-uri", new QoreStringNode(hdr), nullptr);
        }

        // issue #4983: do not sent a "Content-Length: 0" header with GET, HEAD, and TRACE requests
        bool addsize = strcmp(method, "GET") && strcmp(method, "HEAD") && strcmp(method, "TRACE");
        getSendHttpMessageHeadersCommon(hdr, info, headers, size, source, addsize);
    }

    DLLLOCAL void getSendHttpMessageHeadersCommon(QoreString& hdr, QoreHashNode* info, const QoreHashNode* headers,
            size_t size, int source, bool addsize = true, bool add_chunked = false) {
        // send event
        do_send_http_message_event(hdr, headers, source);

        // add headers
        hdr.concat("\r\n");
        // insert headers
        do_headers(hdr, headers, size, addsize, add_chunked);
    }

    DLLLOCAL static void do_accept_encoding(char* t, QoreHashNode& info) {
        ReferenceHolder<QoreListNode> l(new QoreListNode(autoTypeInfo), 0);

        char* a = t;
        bool ok = true;
        while (*a) {
            if (ok) {
                ok = false;
                SimpleRefHolder<QoreStringNode> str(new QoreStringNode);
                while (*a && *a != ';' && *a != ',')
                    str->concat(*(a++));
                str->trim();
                if (!str->empty())
                    l->push(str.release(), nullptr);
                continue;
            }
            else if (*a == ',')
                ok = true;

            ++a;
        }

        if (!l->empty())
            info.setKeyValue("accept-encoding", l.release(), 0);
    }

    DLLLOCAL bool do_accept_charset(char* t, QoreHashNode& info) {
        bool acceptcharset = false;

        // see if we have "*" or utf8 or utf-8, in which case set it
        // otherwise set the first charset in the list
        char* a = t;
        char* div = 0;
        bool utf8 = false;
        bool ok = true;
        while (*a) {
            if (ok) {
                if (*a == '*') {
                    utf8 = true;
                    break;
                }
                ok = false;
                if (*a == 'u' || *a == 'U') {
                    ++a;
                    if (*a == 't' || *a == 'T') {
                        ++a;
                        if (*a == 'f' || *a == 'F') {
                            ++a;
                            if (*a == '-')
                                ++a;
                            if (*a == '8') {
                                utf8 = true;
                                break;
                            }
                        }
                    }
                    continue;
                }
            } else if (*a == ',') {
                if (!div)
                    div = a;
                ok = true;
            } else if (*a == ';') {
                if (!div)
                    div = a;
            }

            ++a;
        }
        if (utf8) {
            info.setKeyValue("accept-charset", new QoreStringNode("utf8"), 0);
            acceptcharset = true;
        } else {
            SimpleRefHolder<QoreStringNode> ac(new QoreStringNode);
            if (div)
                ac->concat(t, div - t);
            else
                ac->concat(t);
            ac->trim();
            if (!ac->empty()) {
                info.setKeyValue("accept-charset", ac.release(), 0);
                acceptcharset = true;
            }
        }

        return acceptcharset;
    }

    // returns true if the connection should be closed, false if not
    DLLLOCAL bool convertHeaderToHash(QoreHashNode* h, char* p, int flags = 0, QoreHashNode* info = nullptr,
            bool* chunked = nullptr, const char* headers_raw_key = "headers-raw") {
        bool close = !(flags & CHF_HTTP11);
        // socket encoding
        const char* senc = nullptr;
        // accept-charset
        bool acceptcharset = false;

        QoreHashNode* raw_hdr = nullptr;
        if (info) {
            info->setKeyValue(headers_raw_key, raw_hdr = new QoreHashNode(autoTypeInfo), nullptr);
        }

        // raw key for setting raw headers
        std::string raw_key;

        while (*p) {
            char* buf = p;

            if ((p = strstr(buf, "\r\n"))) {
                *p = '\0';
                p += 2;
            } else if ((p = strchr(buf, '\n'))) {
                *p = '\0';
                p++;
            } else if ((p = strchr(buf, '\r'))) {
                *p = '\0';
                p++;
            } else
                break;
            char* t = strchr(buf, ':');
            if (!t)
                break;
            *t = '\0';
            t++;
            while (t && qore_isblank(*t))
                t++;
            if (raw_hdr) {
                raw_key = buf;
            }
            strtolower(buf);
            //printd(5, "setting %s = '%s'\n", buf, t);

            ReferenceHolder<> val(new QoreStringNode(t), nullptr);

            if (flags & CHF_PROCESS) {
                if (!strcmp(buf, "connection")) {
                    if (flags & CHF_HTTP11) {
                        if (strcasestr(t, "close"))
                            close = true;
                    } else {
                        if (strcasestr(t, "keep-alive"))
                            close = false;
                    }
                } else if (!strcmp(buf, "content-type")) {
                    char* a = strcasestr(t, "charset=");
                    if (a) {
                        // find end
                        char* e = strchr(a + 8, ';');

                        QoreString cs;
                        if (e) {
                            cs.concat(a + 8, e - a - 8);
                        } else {
                            cs.concat(a + 8);
                        }
                        cs.trim();
                        senc = cs.c_str();
                        //printd(5, "got encoding '%s' from request\n", senc);
                        enc = QEM.findCreate(senc);

                        if (info) {
                            size_t len = cs.size();
                            info->setKeyValue("charset", new QoreStringNode(cs.giveBuffer(), len, len + 1,
                                QCS_DEFAULT), nullptr);
                        }

                        if (info) {
                            SimpleRefHolder<QoreStringNode> ct(new QoreStringNode);
                            // remove any whitespace and ';' before charset=
                            if (a != t) {
                                do {
                                    --a;
                                } while (a > t && (*a == ' ' || *a == ';'));
                            }

                            if (a == t) {
                                if (e) {
                                    ct->concat(e + 1);
                                }
                            } else {
                                ct->concat(t, a - t + 1);
                                if (e) {
                                    ct->concat(e);
                                }
                            }
                            ct->trim();
                            if (!ct->empty()) {
                                info->setKeyValue("body-content-type", ct.release(), nullptr);
                            }
                        }
                    } else {
                        // check for content types that imply UTF-8 encoding
                        std::string ct = t;
                        ct = ct.substr(0, ct.find(';'));
                        if (utf8_content_type_set.find(ct) != utf8_content_type_set.end()) {
                            senc = "UTF-8";
                            enc = QCS_UTF8;
                        } else {
                            senc = assume_http_encoding.c_str();
                            enc = QEM.findCreate(assume_http_encoding.c_str());
                        }
                        if (info) {
                            info->setKeyValue("charset", new QoreStringNode(senc), nullptr);
                            info->setKeyValue("body-content-type", val->refSelf(), nullptr);
                        }
                    }
                } else if (chunked && !strcmp(buf, "transfer-encoding") && !strcasecmp(t, "chunked")) {
                    *chunked = true;
                } else if (info) {
                    if (!strcmp(buf, "accept-charset")) {
                        acceptcharset = do_accept_charset(t, *info);
                    } else if ((flags & CHF_REQUEST) && !strcmp(buf, "accept-encoding")) {
                        do_accept_encoding(t, *info);
                    }
                }
            }

            ReferenceHolder<> val_copy(nullptr);
            if (raw_hdr && val) {
                val_copy = val->realCopy();
            }

            // see if header exists, and if so make it a list and add value to the list
            hash_assignment_priv ha(*h, buf);
            if (!(*ha).isNothing()) {
                QoreListNode* l;
                if ((*ha).getType() == NT_LIST) {
                    l = (*ha).get<QoreListNode>();
                } else {
                    l = new QoreListNode(autoTypeInfo);
                    l->push(ha.swap(l), nullptr);
                }
                l->push(val.release(), nullptr);
            } else // otherwise set header normally
                ha.assign(val.release(), 0);

            // set raw headers if applicable
            if (raw_hdr) {
                hash_assignment_priv ha(*raw_hdr, raw_key);
                if (!(*ha).isNothing()) {
                    QoreListNode* l;
                    if ((*ha).getType() == NT_LIST) {
                        l = (*ha).get<QoreListNode>();
                    } else {
                        l = new QoreListNode(autoTypeInfo);
                        l->push(ha.swap(l), nullptr);
                    }
                    l->push(val_copy.release(), nullptr);
                } else // otherwise set header normally
                    ha.assign(val_copy.release(), nullptr);
            }
        }

        if ((flags & CHF_PROCESS)) {
            if (!senc) {
                enc = QEM.findCreate(assume_http_encoding.c_str());
            }
            // according to RFC-2616 section 14.2, "If no Accept-Charset header is present, the default is that any
            // character set is acceptable" so we will use utf-8
            if (info && !acceptcharset) {
                info->setKeyValue("accept-charset", new QoreStringNode("utf8"), nullptr);
            }
        }

        return close;
    }

    DLLLOCAL void clearWarningQueue(ExceptionSink* xsink) {
        Queue* old_queue = nullptr;
        QoreValue old_arg;
        {
            AutoLocker al(warning_queue_m);
            old_queue = warn_queue;
            old_arg = warn_callback_arg;
            warn_queue = nullptr;
            warn_callback_arg = QoreValue();
            tl_warning_us.store(0, std::memory_order_relaxed);
            tp_warning_bs.store(0.0, std::memory_order_relaxed);
            tp_us_min.store(0, std::memory_order_relaxed);
        }
        if (old_arg) {
            old_arg.discard(xsink);
        }
        if (old_queue) {
            old_queue->deref(xsink);
        }
    }

    DLLLOCAL void setWarningQueue(ExceptionSink* xsink, int64 warning_ms, int64 warning_bs, Queue* wq, QoreValue arg,
            int64 min_ms = 1000) {
        ReferenceHolder<Queue> qholder(wq, xsink);
        ValueHolder holder(arg, xsink);
        if (warning_ms <= 0 && warning_bs <= 0) {
            xsink->raiseException("SOCKET-SETWARNINGQUEUE-ERROR", "Socket::setWarningQueue() at least one of warning "
                "ms argument: " QLLD " and warning B/s argument: " QLLD " must be greater than zero; to clear, call "\
                "Socket::clearWarningQueue() with no arguments", warning_ms, warning_bs);
            return;
        }

        if (warning_ms < 0)
            warning_ms = 0;
        if (warning_bs < 0)
            warning_bs = 0;

        Queue* old_queue = nullptr;
        QoreValue old_arg;
        {
            AutoLocker al(warning_queue_m);
            old_queue = warn_queue;
            old_arg = warn_callback_arg;
            warn_queue = qholder.release();
            warn_callback_arg = holder.release();
            tl_warning_us.store((int64)warning_ms * 1000, std::memory_order_relaxed);
            tp_warning_bs.store((double)warning_bs, std::memory_order_relaxed);
            tp_us_min.store(min_ms * 1000, std::memory_order_relaxed);
        }
        if (old_queue) {
            old_queue->deref(xsink);
        }
        old_arg.discard(xsink);
    }

    DLLLOCAL void swapWarningQueueState(qore_socket_private& s) {
        if (&s == this) {
            return;
        }

        qore_socket_private* first;
        qore_socket_private* second;
        if (std::less<qore_socket_private*>()(this, &s)) {
            first = this;
            second = &s;
        } else {
            first = &s;
            second = this;
        }

        AutoLocker al(first->warning_queue_m);
        AutoLocker bl(second->warning_queue_m);

        std::swap(warn_queue, s.warn_queue);
        std::swap(warn_callback_arg, s.warn_callback_arg);

        int64 tl = tl_warning_us.load(std::memory_order_relaxed);
        tl_warning_us.store(s.tl_warning_us.load(std::memory_order_relaxed), std::memory_order_relaxed);
        s.tl_warning_us.store(tl, std::memory_order_relaxed);

        double tp = tp_warning_bs.load(std::memory_order_relaxed);
        tp_warning_bs.store(s.tp_warning_bs.load(std::memory_order_relaxed), std::memory_order_relaxed);
        s.tp_warning_bs.store(tp, std::memory_order_relaxed);

        int64 min_us = tp_us_min.load(std::memory_order_relaxed);
        tp_us_min.store(s.tp_us_min.load(std::memory_order_relaxed), std::memory_order_relaxed);
        s.tp_us_min.store(min_us, std::memory_order_relaxed);
    }

    DLLLOCAL void getUsageInfo(QoreHashNode& h, qore_socket_private& s) const {
        {
            AutoLocker al(warning_queue_m);
            if (warn_queue) {
                h.setKeyValue("arg", warn_callback_arg.refSelf(), 0);
                h.setKeyValue("timeout", tl_warning_us.load(std::memory_order_relaxed), 0);
                h.setKeyValue("min_throughput", (int64)tp_warning_bs.load(std::memory_order_relaxed), 0);
                h.setKeyValue("min_throughput_us", tp_us_min.load(std::memory_order_relaxed), 0);
            }
        }

        h.setKeyValue("bytes_sent", tp_bytes_sent.load(std::memory_order_relaxed)
            + s.tp_bytes_sent.load(std::memory_order_relaxed), 0);
        h.setKeyValue("bytes_recv", tp_bytes_recv.load(std::memory_order_relaxed)
            + s.tp_bytes_recv.load(std::memory_order_relaxed), 0);
        h.setKeyValue("us_sent", tp_us_sent.load(std::memory_order_relaxed)
            + s.tp_us_sent.load(std::memory_order_relaxed), 0);
        h.setKeyValue("us_recv", tp_us_recv.load(std::memory_order_relaxed)
            + s.tp_us_recv.load(std::memory_order_relaxed), 0);
    }

    DLLLOCAL void getUsageInfo(QoreHashNode& h) const {
        {
            AutoLocker al(warning_queue_m);
            if (warn_queue) {
                h.setKeyValue("arg", warn_callback_arg.refSelf(), 0);
                h.setKeyValue("timeout", tl_warning_us.load(std::memory_order_relaxed), 0);
                h.setKeyValue("min_throughput", (int64)tp_warning_bs.load(std::memory_order_relaxed), 0);
                h.setKeyValue("min_throughput_us", tp_us_min.load(std::memory_order_relaxed), 0);
            }
        }

        h.setKeyValue("bytes_sent", tp_bytes_sent.load(std::memory_order_relaxed), 0);
        h.setKeyValue("bytes_recv", tp_bytes_recv.load(std::memory_order_relaxed), 0);
        h.setKeyValue("us_sent", tp_us_sent.load(std::memory_order_relaxed), 0);
        h.setKeyValue("us_recv", tp_us_recv.load(std::memory_order_relaxed), 0);
    }

    DLLLOCAL QoreHashNode* getUsageInfo() const {
        QoreHashNode* h = new QoreHashNode(autoTypeInfo);
        getUsageInfo(*h);
        return h;
    }

    DLLLOCAL void clearStats() {
        tp_bytes_sent.store(0, std::memory_order_relaxed);
        tp_bytes_recv.store(0, std::memory_order_relaxed);
        tp_us_sent.store(0, std::memory_order_relaxed);
        tp_us_recv.store(0, std::memory_order_relaxed);
    }

    DLLLOCAL void doTimeoutWarning(const char* op, int64 dt) {
        AutoLocker al(warning_queue_m);
        if (!warn_queue) {
            return;
        }
        int64 warning_us = tl_warning_us.load(std::memory_order_relaxed);
        if (!warning_us || dt < warning_us) {
            return;
        }

        QoreHashNode* h = new QoreHashNode(autoTypeInfo);

        h->setKeyValue("type", new QoreStringNode("SOCKET-OPERATION-WARNING"), 0);
        h->setKeyValue("operation", new QoreStringNode(op), 0);
        h->setKeyValue("us", dt, 0);
        h->setKeyValue("timeout", warning_us, 0);
        if (warn_callback_arg)
            h->setKeyValue("arg", warn_callback_arg.refSelf(), 0);

        warn_queue->pushAndTakeRef(h);
    }

    DLLLOCAL void doThroughputWarning(bool send, int64 bytes, int64 dt, double bs) {
        AutoLocker al(warning_queue_m);
        if (!warn_queue) {
            return;
        }
        double warning_bs = tp_warning_bs.load(std::memory_order_relaxed);
        if (!warning_bs || bs > warning_bs) {
            return;
        }

        QoreHashNode* h = new QoreHashNode(autoTypeInfo);

        h->setKeyValue("type", new QoreStringNode("SOCKET-THROUGHPUT-WARNING"), 0);
        h->setKeyValue("dir", new QoreStringNode(send ? "send" : "recv"), 0);
        h->setKeyValue("bytes", bytes, 0);
        h->setKeyValue("us", dt, 0);
        h->setKeyValue("bytes_sec", bs, 0);
        h->setKeyValue("threshold", (int64)warning_bs, 0);
        if (warn_callback_arg)
            h->setKeyValue("arg", warn_callback_arg.refSelf(), 0);

        warn_queue->pushAndTakeRef(h);
    }

    DLLLOCAL bool pendingHttpChunkedBody() const {
        return http_exp_chunked_body && sock != QORE_INVALID_SOCKET;
    }

    DLLLOCAL void setSslVerifyMode(int mode) {
        //printd(5, "qore_socket_private::setSslVerifyMode() this: %p mode: %d\n", this, mode);
        ssl_verify_mode = mode;
        if (ssl)
            ssl->setVerifyMode(ssl_verify_mode, ssl_accept_all_certs, client_target);
    }

    DLLLOCAL void acceptAllCertificates(bool accept_all = true) {
        ssl_accept_all_certs = accept_all;
        if (ssl)
            ssl->setVerifyMode(ssl_verify_mode, ssl_accept_all_certs, client_target);
    }

    DLLLOCAL void setSslErrorString(QoreStringNode* err_str) {
        if (ssl_err_str) {
            ssl_err_str->concat("; ");
            qore_string_private::get(ssl_err_str)->concat(err_str);
            err_str->deref();
        } else {
            ssl_err_str = err_str;
        }
    }

    DLLLOCAL static void getUsageInfo(const QoreSocket& sock, QoreHashNode& h, const QoreSocket& s) {
        sock.priv->getUsageInfo(h, *s.priv);
    }

    DLLLOCAL static qore_socket_private* get(QoreSocket& sock) {
        return sock.priv;
    }

    DLLLOCAL static const qore_socket_private* get(const QoreSocket& sock) {
        return sock.priv;
    }

    DLLLOCAL static void captureRemoteCert(X509_STORE_CTX* x509_ctx);

    DLLLOCAL static QoreListNode* poll(const QoreListNode* poll_list, int timeout_ms, ExceptionSink* xsink);
};

#endif

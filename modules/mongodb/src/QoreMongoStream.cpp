/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreMongoStream.cpp

    Qore mongodb module - Interruptible stream wrapper implementation

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
*/

#include "QoreMongoStream.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <new>
#include <vector>

static bool qore_mongoc_verbose_enabled() {
    const char* env = getenv("QORE_MONGODB_VERBOSE");
    if (!env || !*env) {
        return false;
    }
    char* end = nullptr;
    long level = strtol(env, &end, 10);
    if (end == env) {
        return false;
    }
    return level > 2;
}

static void qore_mongoc_log_handler(mongoc_log_level_t level, const char* domain, const char* message,
        void* user_data) {
    if ((level == MONGOC_LOG_LEVEL_DEBUG || level == MONGOC_LOG_LEVEL_TRACE) && !qore_mongoc_verbose_enabled()) {
        return;
    }
    mongoc_log_default_handler(level, domain, message, user_data);
}

void qore_mongo_set_log_handler() {
    mongoc_log_set_handler(qore_mongoc_log_handler, nullptr);
}

//! the stream type of the module's socket stream; libmongoc's own types are small numbers
static constexpr int QORE_MONGO_SOCKET_STREAM_TYPE = 100;

//! A libmongoc stream on a socket of the module's own whose waits end as soon as the thread is cancelled
/** libmongoc's own socket stream waits inside libmongoc on a descriptor that its public API does not expose, so a
    wait could only notice a cancellation request at a periodic timeout.  This stream owns its (non-blocking)
    descriptor and waits with qore_cancellable_poll(), which a cancellation request or program interrupt ends at
    once.  libmongoc's TLS stream is stacked on it for TLS connections, and libmongoc's mongoc_stream_poll() calls
    the poll function of the root stream, which is this one, also for TLS streams.
*/
typedef struct {
    mongoc_stream_t vtable;     //!< must be first: the stream vtable
    int fd;                     //!< the socket, or -1 once closed
    int last_errno;             //!< the errno of the last failed I/O, for timed_out() and should_retry()
} qore_socket_stream_t;

//! returns the absolute deadline in microseconds for a libmongoc timeout: -1 for none, 0 for "do not wait"
static int64 qore_mongo_expiration(int32_t timeout_msec) {
    if (timeout_msec < 0) {
        return -1;
    }
    if (!timeout_msec) {
        return 0;
    }
    return q_get_monotonic_us() + static_cast<int64>(timeout_msec) * 1000;
}

//! returns the milliseconds remaining until a deadline from qore_mongo_expiration() for poll()
static int qore_mongo_remaining_ms(int64 expire_at) {
    if (expire_at < 0) {
        return -1;
    }
    if (!expire_at) {
        return 0;
    }
    int64 remaining_us = expire_at - q_get_monotonic_us();
    return remaining_us <= 0 ? 0 : static_cast<int>((remaining_us + 999) / 1000);
}

//! Waits for a descriptor; the wait ends at once when the thread is cancelled or its Program is interrupted
/** @return 0 if the descriptor is ready, -1 with errno set otherwise: \c ETIMEDOUT on timeout, \c EINTR if the
    thread was cancelled or interrupted (the request stays pending and is raised at the next cancellation point)
*/
static int qore_mongo_wait(int fd, short events, int64 expire_at, const char* operation) {
    struct pollfd pfd = {fd, events, 0};
    ExceptionSink xsink;
    int rc = qore_cancellable_poll(&pfd, 1, qore_mongo_remaining_ms(expire_at), &xsink, operation);
    if (rc == QORE_POLL_CANCELLED) {
        // libmongoc reports errors through errno; the request itself remains pending
        xsink.clear();
        errno = EINTR;
        return -1;
    }
    if (!rc) {
        errno = ETIMEDOUT;
        return -1;
    }
    return rc < 0 ? -1 : 0;
}

static void qore_socket_stream_close_fd(qore_socket_stream_t* s) {
    if (s->fd != -1) {
        close(s->fd);
        s->fd = -1;
    }
}

static void qore_socket_stream_destroy(mongoc_stream_t* stream) {
    qore_socket_stream_t* s = reinterpret_cast<qore_socket_stream_t*>(stream);
    qore_socket_stream_close_fd(s);
    bson_free(s);
}

static int qore_socket_stream_close(mongoc_stream_t* stream) {
    qore_socket_stream_close_fd(reinterpret_cast<qore_socket_stream_t*>(stream));
    return 0;
}

static int qore_socket_stream_flush(mongoc_stream_t* stream) {
    return 0;
}

//! Writes all of the data, or fails; like libmongoc's socket stream
static ssize_t qore_socket_stream_writev(mongoc_stream_t* stream, mongoc_iovec_t* iov, size_t iovcnt,
        int32_t timeout_msec) {
    qore_socket_stream_t* s = reinterpret_cast<qore_socket_stream_t*>(stream);
    if (s->fd == -1) {
        errno = s->last_errno = EBADF;
        return -1;
    }
    int64 expire_at = qore_mongo_expiration(timeout_msec);
    ssize_t total = 0;
    for (size_t i = 0; i < iovcnt; ++i) {
        const char* buf = static_cast<const char*>(iov[i].iov_base);
        size_t off = 0;
        while (off < iov[i].iov_len) {
#ifdef MSG_NOSIGNAL
            ssize_t n = send(s->fd, buf + off, iov[i].iov_len - off, MSG_NOSIGNAL);
#else
            ssize_t n = send(s->fd, buf + off, iov[i].iov_len - off, 0);
#endif
            if (n >= 0) {
                off += n;
                total += n;
                continue;
            }
            if (errno == EINTR) {
                continue;
            }
            if ((errno == EAGAIN || errno == EWOULDBLOCK) && expire_at
                && !qore_mongo_wait(s->fd, POLLOUT, expire_at, "MongoDB write")) {
                continue;
            }
            s->last_errno = errno;
            return -1;
        }
    }
    s->last_errno = 0;
    return total;
}

//! Reads until at least \a min_bytes have been read; like libmongoc's socket stream
static ssize_t qore_socket_stream_readv(mongoc_stream_t* stream, mongoc_iovec_t* iov, size_t iovcnt,
        size_t min_bytes, int32_t timeout_msec) {
    qore_socket_stream_t* s = reinterpret_cast<qore_socket_stream_t*>(stream);
    if (s->fd == -1) {
        errno = s->last_errno = EBADF;
        return -1;
    }
    int64 expire_at = qore_mongo_expiration(timeout_msec);
    ssize_t total = 0;
    for (size_t i = 0; i < iovcnt; ++i) {
        char* buf = static_cast<char*>(iov[i].iov_base);
        size_t off = 0;
        while (off < iov[i].iov_len) {
            ssize_t n = recv(s->fd, buf + off, iov[i].iov_len - off, 0);
            if (n > 0) {
                off += n;
                total += n;
                if (static_cast<size_t>(total) >= min_bytes) {
                    s->last_errno = 0;
                    return total;
                }
                continue;
            }
            if (!n) {
                // the peer closed the connection
                if (static_cast<size_t>(total) >= min_bytes) {
                    return total;
                }
                errno = s->last_errno = ECONNRESET;
                return -1;
            }
            if (errno == EINTR) {
                continue;
            }
            if ((errno == EAGAIN || errno == EWOULDBLOCK) && expire_at
                && !qore_mongo_wait(s->fd, POLLIN, expire_at, "MongoDB read")) {
                continue;
            }
            s->last_errno = errno;
            if (static_cast<size_t>(total) >= min_bytes && total) {
                return total;
            }
            return -1;
        }
    }
    s->last_errno = 0;
    return total;
}

static int qore_socket_stream_setsockopt(mongoc_stream_t* stream, int level, int optname, void* optval,
        mongoc_socklen_t optlen) {
    qore_socket_stream_t* s = reinterpret_cast<qore_socket_stream_t*>(stream);
    if (s->fd == -1) {
        errno = EBADF;
        return -1;
    }
    return setsockopt(s->fd, level, optname, optval, optlen);
}

//! Returns true if the connection has been closed by the peer or has failed
static bool qore_socket_stream_check_closed(mongoc_stream_t* stream) {
    qore_socket_stream_t* s = reinterpret_cast<qore_socket_stream_t*>(stream);
    if (s->fd == -1) {
        return true;
    }
    struct pollfd pfd = {s->fd, POLLIN, 0};
    int rc;
    while ((rc = poll(&pfd, 1, 0)) < 0 && errno == EINTR) {
    }
    if (rc < 0) {
        return true;
    }
    if (!rc) {
        return false;
    }
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        return true;
    }
    char c;
    ssize_t n;
    while ((n = recv(s->fd, &c, 1, MSG_PEEK)) < 0 && errno == EINTR) {
    }
    if (!n) {
        return true;
    }
    return n < 0 && errno != EAGAIN && errno != EWOULDBLOCK;
}

//! Polls the module's socket streams; called by mongoc_stream_poll() with the root streams
static ssize_t qore_socket_stream_poll(mongoc_stream_poll_t* streams, size_t nstreams, int32_t timeout) {
    struct pollfd local_pfds[4];
    std::vector<struct pollfd> heap_pfds;
    struct pollfd* pfds = local_pfds;
    if (nstreams > sizeof local_pfds / sizeof local_pfds[0]) {
        try {
            heap_pfds.resize(nstreams);
        } catch (std::bad_alloc&) {
            errno = ENOMEM;
            return -1;
        }
        pfds = heap_pfds.data();
    }
    for (size_t i = 0; i < nstreams; ++i) {
        assert(streams[i].stream->type == QORE_MONGO_SOCKET_STREAM_TYPE);
        pfds[i].fd = reinterpret_cast<qore_socket_stream_t*>(streams[i].stream)->fd;
        pfds[i].events = static_cast<short>(streams[i].events);
        pfds[i].revents = 0;
    }
    ExceptionSink xsink;
    int rc = qore_cancellable_poll(pfds, static_cast<unsigned>(nstreams), timeout < 0 ? -1 : timeout, &xsink,
        "MongoDB poll");
    if (rc == QORE_POLL_CANCELLED) {
        // libmongoc's async loop (the topology scanner, which runs the connection handshake) treats a failed poll
        // as nothing ready and polls again until the command's own timeout, so failing here would spin until
        // then; reporting every stream as failed ends its commands at once, and their I/O then fails with EINTR
        // (the request stays pending and is raised at the next cancellation point)
        xsink.clear();
        for (size_t i = 0; i < nstreams; ++i) {
            streams[i].revents = POLLERR | POLLHUP;
        }
        return static_cast<ssize_t>(nstreams);
    }
    if (rc > 0) {
        for (size_t i = 0; i < nstreams; ++i) {
            streams[i].revents = pfds[i].revents;
        }
    }
    return rc;
}

//! A wait that ended because of a cancellation request is not a timeout
static bool qore_socket_stream_timed_out(mongoc_stream_t* stream) {
    if (qore_check_cancel(nullptr, "MongoDB I/O")) {
        return false;
    }
    return reinterpret_cast<qore_socket_stream_t*>(stream)->last_errno == ETIMEDOUT;
}

//! An operation that was cancelled is never retried
static bool qore_socket_stream_should_retry(mongoc_stream_t* stream) {
    if (qore_check_cancel(nullptr, "MongoDB I/O")) {
        return false;
    }
    int e = reinterpret_cast<qore_socket_stream_t*>(stream)->last_errno;
    return e == EAGAIN || e == EWOULDBLOCK;
}

//! Creates a stream that takes ownership of a connected non-blocking socket
static mongoc_stream_t* qore_socket_stream_new(int fd) {
    qore_socket_stream_t* s = static_cast<qore_socket_stream_t*>(bson_malloc0(sizeof(qore_socket_stream_t)));
    s->vtable.type = QORE_MONGO_SOCKET_STREAM_TYPE;
    s->vtable.destroy = qore_socket_stream_destroy;
    s->vtable.close = qore_socket_stream_close;
    s->vtable.flush = qore_socket_stream_flush;
    s->vtable.writev = qore_socket_stream_writev;
    s->vtable.readv = qore_socket_stream_readv;
    s->vtable.setsockopt = qore_socket_stream_setsockopt;
    // no get_base_stream function: this is the root stream (mongoc_stream_get_root_stream() stops at a stream
    // without one)
    s->vtable.check_closed = qore_socket_stream_check_closed;
    s->vtable.poll = qore_socket_stream_poll;
    s->vtable.timed_out = qore_socket_stream_timed_out;
    s->vtable.should_retry = qore_socket_stream_should_retry;
    s->fd = fd;
    return reinterpret_cast<mongoc_stream_t*>(s);
}

//! Connects a new non-blocking, close-on-exec socket; the wait ends at once on cancellation
/** @return the connected socket, or -1 with errno set (\c EINTR if the thread was cancelled or interrupted)
*/
static int qore_mongo_connect(const struct sockaddr* sa, mongoc_socklen_t addrlen, int32_t timeout_msec) {
    int fd = socket(sa->sa_family, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        return -1;
    }
    // closes the socket unless it is released
    struct FdHolder {
        int fd;
        ~FdHolder() {
            if (fd != -1) {
                int err = errno;
                close(fd);
                errno = err;
            }
        }
    } holder{fd};

    int fdflags = fcntl(fd, F_GETFD);
    int flflags = fdflags < 0 ? -1 : fcntl(fd, F_GETFL);
    if (flflags < 0 || fcntl(fd, F_SETFD, fdflags | FD_CLOEXEC) < 0
        || fcntl(fd, F_SETFL, flflags | O_NONBLOCK) < 0) {
        return -1;
    }
    int one = 1;
    // the same options as libmongoc's own sockets; failures are not fatal, as there
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof one);
#ifdef SO_NOSIGPIPE
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif

    if (connect(fd, sa, addrlen)) {
        if (errno != EINPROGRESS && errno != EINTR) {
            return -1;
        }
        if (qore_mongo_wait(fd, POLLOUT, qore_mongo_expiration(timeout_msec), "MongoDB connection")) {
            return -1;
        }
        int so_error = 0;
        socklen_t len = sizeof so_error;
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len)) {
            return -1;
        }
        if (so_error) {
            errno = so_error;
            return -1;
        }
    }
    holder.fd = -1;
    return fd;
}

static const QoreHashNode* qore_mongo_get_hash(const QoreValue& v) {
    return v.getType() == NT_HASH ? v.get<const QoreHashNode>() : nullptr;
}

// note: the value can be held in inline short string storage, which has no QoreStringNode, so the
// node value helper is used to materialize such values; the caller owns the returned reference
static QoreStringNode* qore_mongo_get_hash_string_value(const QoreHashNode& h, const char* key) {
    QoreValue v = h.getKeyValue(key);
    if (v.getType() != NT_STRING) {
        return nullptr;
    }
    return QoreStringNodeValueHelper(v).getReferencedValue();
}

static int qore_mongo_get_hash_int_value(const QoreHashNode& h, const char* key, int def = 0) {
    QoreValue v = h.getKeyValue(key);
    return v.isNullOrNothing() ? def : static_cast<int>(v.getAsBigInt());
}

static int qore_mongo_addrinfo_hash_to_sockaddr(const QoreHashNode& h, uint16_t default_port,
        struct sockaddr_storage& addr, mongoc_socklen_t& addrlen) {
    SimpleRefHolder<QoreStringNode> address(qore_mongo_get_hash_string_value(h, "address"));
    if (!address) {
        return -1;
    }

    int family = qore_mongo_get_hash_int_value(h, "family", AF_UNSPEC);
    uint16_t port = static_cast<uint16_t>(qore_mongo_get_hash_int_value(h, "port", default_port));
    memset(&addr, 0, sizeof(addr));

    if (family == AF_INET) {
        struct sockaddr_in* in = reinterpret_cast<struct sockaddr_in*>(&addr);
        in->sin_family = AF_INET;
        in->sin_port = htons(port);
        if (inet_pton(AF_INET, address->c_str(), &in->sin_addr) != 1) {
            return -1;
        }
        addrlen = sizeof(struct sockaddr_in);
        return 0;
    }
    if (family == AF_INET6) {
        struct sockaddr_in6* in6 = reinterpret_cast<struct sockaddr_in6*>(&addr);
        in6->sin6_family = AF_INET6;
        in6->sin6_port = htons(port);
        if (inet_pton(AF_INET6, address->c_str(), &in6->sin6_addr) != 1) {
            return -1;
        }
        addrlen = sizeof(struct sockaddr_in6);
        return 0;
    }

    return -1;
}

// note: the description can be held in inline short string storage, which has no QoreStringNode;
// the bytes are copied out because the data helper's buffer does not outlive this call
static std::string qore_mongo_exception_desc(ExceptionSink& xsink, const char* fallback) {
    QoreStringDataHelper desc(xsink.getExceptionDesc());
    return desc ? std::string(desc.c_str(), desc.size()) : std::string(fallback);
}

// applies the TLS options the URI carries over libmongoc's defaults
/** This initiator is installed with mongoc_client_set_stream_initiator(), which replaces
    libmongoc's own initiator and with it the code that reads these options out of the URI, so an
    option a user wrote in the connection string would otherwise have no effect at all.
*/
static void qore_mongo_apply_uri_tls_opts(const mongoc_uri_t* uri, mongoc_ssl_opt_t& opt) {
    if (const char* v = mongoc_uri_get_option_as_utf8(uri, MONGOC_URI_TLSCERTIFICATEKEYFILE, nullptr)) {
        opt.pem_file = v;
    }
    if (const char* v = mongoc_uri_get_option_as_utf8(uri, MONGOC_URI_TLSCERTIFICATEKEYFILEPASSWORD,
            nullptr)) {
        opt.pem_pwd = v;
    }
    if (const char* v = mongoc_uri_get_option_as_utf8(uri, MONGOC_URI_TLSCAFILE, nullptr)) {
        opt.ca_file = v;
    }

    // "tlsInsecure" is the documented shorthand for both of the checks below
    bool insecure = mongoc_uri_get_option_as_bool(uri, MONGOC_URI_TLSINSECURE, false);
    opt.weak_cert_validation = insecure
        || mongoc_uri_get_option_as_bool(uri, MONGOC_URI_TLSALLOWINVALIDCERTIFICATES, false);
    opt.allow_invalid_hostname = insecure
        || mongoc_uri_get_option_as_bool(uri, MONGOC_URI_TLSALLOWINVALIDHOSTNAMES, false);
}

mongoc_stream_t* qore_mongo_stream_initiator(
    const mongoc_uri_t* uri,
    const mongoc_host_list_t* host,
    void* user_data,
    bson_error_t* error) {

    // Check for interrupt/cancel before starting connection
    if (qore_check_cancel(nullptr, "MongoDB connection")) {
        bson_set_error(error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_CONNECT,
            "MongoDB connection interrupted");
        return nullptr;
    }

    // a policy check honors an active policy barrier
    QoreSandboxManagerHelper smh(QoreSandboxManagerHelper::Policy);

    // Resolve the hostname
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", host->port);

    ExceptionSink resolve_xsink;
    ReferenceHolder<QoreListNode> addrs(
        q_getaddrinfo_to_list(&resolve_xsink, host->host, port_str, host->family, 0, SOCK_STREAM), &resolve_xsink);
    if (resolve_xsink || !addrs || addrs->empty()) {
        // note: bson_set_error() is a varargs function, so the description must be held in a
        // named std::string and passed as a const char*
        std::string resolve_desc = resolve_xsink
            ? qore_mongo_exception_desc(resolve_xsink, "name resolution failed")
            : std::string("name resolution returned no addresses");
        bson_set_error(error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_NAME_RESOLUTION,
            "Failed to resolve '%s': %s", host->host, resolve_desc.c_str());
        if (resolve_xsink) {
            resolve_xsink.clear();
        }
        return nullptr;
    }

    // Get connect timeout from URI (default 10 seconds)
    int32_t connecttimeoutms = mongoc_uri_get_option_as_int32(uri, MONGOC_URI_CONNECTTIMEOUTMS, 10000);

    // Try each address until we successfully connect
    int fd = -1;
    ConstListIterator ai(*addrs);

    while (ai.next()) {
        const QoreHashNode* addr_info = qore_mongo_get_hash(ai.getValue());
        if (!addr_info) {
            continue;
        }
        struct sockaddr_storage addr;
        mongoc_socklen_t addrlen = 0;
        if (qore_mongo_addrinfo_hash_to_sockaddr(*addr_info, host->port, addr, addrlen)) {
            continue;
        }
        struct sockaddr* sa = reinterpret_cast<struct sockaddr*>(&addr);

        // Check for interrupt/cancel before each connection attempt
        if (qore_check_cancel(nullptr, "MongoDB connection")) {
            bson_set_error(error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_CONNECT,
                "MongoDB connection interrupted");
            return nullptr;
        }

        // Check network access if sandbox manager is present
        if (smh) {
            ExceptionSink xsink;
            // the host name lets an allowed host pattern apply; the protocol is a sandbox protocol flag
            if (!smh->checkNetworkAccess(host->host, sa, addrlen, QSEC_NET_TCP, &xsink)) {
                // Network access denied by sandbox
                if (xsink) {
                    bson_set_error(error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_CONNECT,
                        "MongoDB connection denied by sandbox: network access restricted");
                    return nullptr;
                }
                // Try next address
                continue;
            }
        }

        fd = qore_mongo_connect(sa, addrlen, connecttimeoutms);
        if (fd != -1) {
            break;  // Success
        }
        if (errno == EINTR) {
            bson_set_error(error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_CONNECT,
                "MongoDB connection interrupted");
            return nullptr;
        }
    }

    if (fd == -1) {
        bson_set_error(error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_CONNECT,
            "Failed to connect to '%s:%u'", host->host, host->port);
        return nullptr;
    }

    // the stream owns the socket from here on
    mongoc_stream_t* base = qore_socket_stream_new(fd);

    // Check if SSL/TLS is required
    if (mongoc_uri_get_tls(uri)) {
        // Check for interrupt/cancel before TLS setup
        if (qore_check_cancel(nullptr, "MongoDB TLS setup")) {
            mongoc_stream_destroy(base);
            bson_set_error(error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_CONNECT,
                "MongoDB TLS setup interrupted");
            return nullptr;
        }

        // NOTE: the options argument is dereferenced by libmongoc and must not be null; passing
        // nullptr here crashed the process on the first TLS connection, which meant every Atlas
        // cluster (mongodb+srv:// implies TLS) was unreachable.  libmongoc's own default initiator
        // passes the client's options, which it fills in from the URI; this initiator replaces that
        // code path entirely, so the URI's TLS options have to be applied here or they are silently
        // ignored
        mongoc_ssl_opt_t ssl_opt = *mongoc_ssl_opt_get_default();
        qore_mongo_apply_uri_tls_opts(uri, ssl_opt);

        mongoc_stream_t* tls_stream = mongoc_stream_tls_new_with_hostname(base, host->host, &ssl_opt, 1);
        if (!tls_stream) {
            mongoc_stream_destroy(base);
            bson_set_error(error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_SOCKET,
                "Failed to create TLS stream for '%s:%u'", host->host, host->port);
            return nullptr;
        }

        // Check for interrupt/cancel before TLS handshake
        if (qore_check_cancel(nullptr, "MongoDB TLS handshake")) {
            mongoc_stream_destroy(tls_stream);
            bson_set_error(error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_CONNECT,
                "MongoDB TLS handshake interrupted");
            return nullptr;
        }

        // Perform TLS handshake
        if (!mongoc_stream_tls_handshake_block(tls_stream, host->host, 10000, error)) {
            mongoc_stream_destroy(tls_stream);
            return nullptr;
        }

        base = tls_stream;
    }

    return base;
}

void qore_mongo_setup_interruptible_streams(mongoc_client_t* client) {
    qore_mongo_set_log_handler();
    mongoc_client_set_stream_initiator(client, qore_mongo_stream_initiator, nullptr);
}

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
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

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

//! Interruptible stream structure
/** This structure wraps the default MongoDB socket stream and adds
    interrupt checking capability.
*/
typedef struct {
    mongoc_stream_t vtable;     //!< Must be first - stream vtable
    mongoc_stream_t* base;      //!< Wrapped base stream
} qore_interruptible_stream_t;

// Forward declarations for stream methods
static void qore_stream_destroy(mongoc_stream_t* stream);
static int qore_stream_close(mongoc_stream_t* stream);
static int qore_stream_flush(mongoc_stream_t* stream);
static ssize_t qore_stream_writev(mongoc_stream_t* stream, mongoc_iovec_t* iov, size_t iovcnt, int32_t timeout_msec);
static ssize_t qore_stream_readv(mongoc_stream_t* stream, mongoc_iovec_t* iov, size_t iovcnt, size_t min_bytes, int32_t timeout_msec);
static int qore_stream_setsockopt(mongoc_stream_t* stream, int level, int optname, void* optval, mongoc_socklen_t optlen);
static mongoc_stream_t* qore_stream_get_base_stream(mongoc_stream_t* stream);
static bool qore_stream_check_closed(mongoc_stream_t* stream);
static ssize_t qore_stream_poll(mongoc_stream_poll_t* streams, size_t nstreams, int32_t timeout);
static bool qore_stream_timed_out(mongoc_stream_t* stream);
static bool qore_stream_should_retry(mongoc_stream_t* stream);

//! Check if an interrupt or thread cancel has been requested
/** @return true if interrupted/cancelled, false otherwise
*/
static bool check_interrupt() {
    if (qore_check_cancel(nullptr, "MongoDB I/O")) {
        errno = EINTR;
        return true;
    }
    return false;
}

//! Destroy the interruptible stream
static void qore_stream_destroy(mongoc_stream_t* stream) {
    qore_interruptible_stream_t* s = (qore_interruptible_stream_t*)stream;
    if (s->base) {
        mongoc_stream_destroy(s->base);
    }
    bson_free(s);
}

//! Close the interruptible stream
static int qore_stream_close(mongoc_stream_t* stream) {
    qore_interruptible_stream_t* s = (qore_interruptible_stream_t*)stream;
    return s->base ? mongoc_stream_close(s->base) : 0;
}

//! Flush the interruptible stream
static int qore_stream_flush(mongoc_stream_t* stream) {
    qore_interruptible_stream_t* s = (qore_interruptible_stream_t*)stream;
    return s->base ? mongoc_stream_flush(s->base) : 0;
}

//! Write to the interruptible stream with interrupt checking
static ssize_t qore_stream_writev(mongoc_stream_t* stream, mongoc_iovec_t* iov, size_t iovcnt, int32_t timeout_msec) {
    qore_interruptible_stream_t* s = (qore_interruptible_stream_t*)stream;

    // Check for interrupt before writing
    if (check_interrupt()) {
        return -1;
    }

    if (!s->base) {
        errno = EBADF;
        return -1;
    }

    // For short timeouts, use direct call
    if (timeout_msec <= QORE_IO_POLL_INTERVAL_MS) {
        return mongoc_stream_writev(s->base, iov, iovcnt, timeout_msec);
    }

    // Always poll with interrupt/cancel checking for long timeouts
    int32_t remaining = timeout_msec;
    while (remaining > 0) {
        if (check_interrupt()) {
            return -1;
        }

        int32_t chunk_timeout = remaining > QORE_IO_POLL_INTERVAL_MS ? QORE_IO_POLL_INTERVAL_MS : remaining;
        ssize_t rv = mongoc_stream_writev(s->base, iov, iovcnt, chunk_timeout);

        if (rv >= 0) {
            return rv;
        }

        // Check if it was a timeout
        if (errno != ETIMEDOUT && errno != EAGAIN && errno != EWOULDBLOCK) {
            return rv;
        }

        remaining -= chunk_timeout;
    }

    errno = ETIMEDOUT;
    return -1;
}

//! Read from the interruptible stream with interrupt checking
static ssize_t qore_stream_readv(mongoc_stream_t* stream, mongoc_iovec_t* iov, size_t iovcnt, size_t min_bytes, int32_t timeout_msec) {
    qore_interruptible_stream_t* s = (qore_interruptible_stream_t*)stream;

    // Check for interrupt before reading
    if (check_interrupt()) {
        return -1;
    }

    if (!s->base) {
        errno = EBADF;
        return -1;
    }

    // For short timeouts, use direct call
    if (timeout_msec <= QORE_IO_POLL_INTERVAL_MS) {
        return mongoc_stream_readv(s->base, iov, iovcnt, min_bytes, timeout_msec);
    }

    // Always poll with interrupt/cancel checking for long timeouts
    int32_t remaining = timeout_msec;
    while (remaining > 0) {
        if (check_interrupt()) {
            return -1;
        }

        int32_t chunk_timeout = remaining > QORE_IO_POLL_INTERVAL_MS ? QORE_IO_POLL_INTERVAL_MS : remaining;
        ssize_t rv = mongoc_stream_readv(s->base, iov, iovcnt, min_bytes, chunk_timeout);

        if (rv >= 0) {
            return rv;
        }

        // Check if it was a timeout
        if (errno != ETIMEDOUT && errno != EAGAIN && errno != EWOULDBLOCK) {
            return rv;
        }

        remaining -= chunk_timeout;
    }

    errno = ETIMEDOUT;
    return -1;
}

//! Set socket option on the interruptible stream
static int qore_stream_setsockopt(mongoc_stream_t* stream, int level, int optname, void* optval, mongoc_socklen_t optlen) {
    qore_interruptible_stream_t* s = (qore_interruptible_stream_t*)stream;
    return s->base ? mongoc_stream_setsockopt(s->base, level, optname, optval, optlen) : -1;
}

//! Get the base stream
static mongoc_stream_t* qore_stream_get_base_stream(mongoc_stream_t* stream) {
    qore_interruptible_stream_t* s = (qore_interruptible_stream_t*)stream;
    return s->base;
}

//! Check if stream is closed
static bool qore_stream_check_closed(mongoc_stream_t* stream) {
    qore_interruptible_stream_t* s = (qore_interruptible_stream_t*)stream;
    return s->base ? mongoc_stream_check_closed(s->base) : true;
}

//! Poll multiple streams with interrupt checking
static ssize_t qore_stream_poll(mongoc_stream_poll_t* streams, size_t nstreams, int32_t timeout) {
    // Check for interrupt before polling
    if (check_interrupt()) {
        return -1;
    }

    // For short timeouts, use direct call
    if (timeout <= QORE_IO_POLL_INTERVAL_MS) {
        return mongoc_stream_poll(streams, nstreams, timeout);
    }

    // Always poll with interrupt/cancel checking
    int32_t remaining = timeout;
    while (remaining > 0) {
        if (check_interrupt()) {
            return -1;
        }

        int32_t chunk_timeout = remaining > QORE_IO_POLL_INTERVAL_MS ? QORE_IO_POLL_INTERVAL_MS : remaining;
        ssize_t rv = mongoc_stream_poll(streams, nstreams, chunk_timeout);

        if (rv != 0) {
            return rv;
        }

        remaining -= chunk_timeout;
    }

    return 0;  // Timeout
}

//! Check if stream timed out
/** Returns false if cancel/interrupt is pending since the operation was
    interrupted, not timed out.
*/
static bool qore_stream_timed_out(mongoc_stream_t* stream) {
    // Not a timeout if cancel/interrupt is pending
    if (qore_check_cancel(nullptr, "MongoDB I/O")) {
        return false;
    }
    qore_interruptible_stream_t* s = (qore_interruptible_stream_t*)stream;
    return s->base ? mongoc_stream_timed_out(s->base) : false;
}

//! Check if stream should retry
/** Returns false if cancel/interrupt is pending to prevent mongoc from
    retrying an operation that was intentionally interrupted.
*/
static bool qore_stream_should_retry(mongoc_stream_t* stream) {
    // Never retry if cancel/interrupt is pending
    if (qore_check_cancel(nullptr, "MongoDB I/O")) {
        return false;
    }
    qore_interruptible_stream_t* s = (qore_interruptible_stream_t*)stream;
    return s->base ? mongoc_stream_should_retry(s->base) : false;
}

//! Create a new interruptible stream wrapping a base stream
static mongoc_stream_t* qore_stream_new(mongoc_stream_t* base) {
    qore_interruptible_stream_t* stream = (qore_interruptible_stream_t*)bson_malloc0(sizeof(qore_interruptible_stream_t));

    stream->vtable.type = 100;  // Custom type
    stream->vtable.destroy = qore_stream_destroy;
    stream->vtable.close = qore_stream_close;
    stream->vtable.flush = qore_stream_flush;
    stream->vtable.writev = qore_stream_writev;
    stream->vtable.readv = qore_stream_readv;
    stream->vtable.setsockopt = qore_stream_setsockopt;
    stream->vtable.get_base_stream = qore_stream_get_base_stream;
    stream->vtable.check_closed = qore_stream_check_closed;
    stream->vtable.poll = qore_stream_poll;
    stream->vtable.timed_out = qore_stream_timed_out;
    stream->vtable.should_retry = qore_stream_should_retry;
    stream->base = base;

    return (mongoc_stream_t*)stream;
}

static const QoreHashNode* qore_mongo_get_hash(const QoreValue& v) {
    return v.getType() == NT_HASH ? v.get<const QoreHashNode>() : nullptr;
}

static const QoreStringNode* qore_mongo_get_hash_string_value(const QoreHashNode& h, const char* key) {
    QoreValue v = h.getKeyValue(key);
    return v.getType() == NT_STRING ? v.get<const QoreStringNode>() : nullptr;
}

static int qore_mongo_get_hash_int_value(const QoreHashNode& h, const char* key, int def = 0) {
    QoreValue v = h.getKeyValue(key);
    return v.isNullOrNothing() ? def : static_cast<int>(v.getAsBigInt());
}

static int qore_mongo_addrinfo_hash_to_sockaddr(const QoreHashNode& h, uint16_t default_port,
        struct sockaddr_storage& addr, mongoc_socklen_t& addrlen) {
    const QoreStringNode* address = qore_mongo_get_hash_string_value(h, "address");
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

static const char* qore_mongo_exception_desc(ExceptionSink& xsink, const char* fallback) {
    QoreValue desc = xsink.getExceptionDesc();
    return desc.getType() == NT_STRING ? desc.get<const QoreStringNode>()->c_str() : fallback;
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

    QoreSandboxManagerHelper smh;

    // Resolve the hostname
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", host->port);

    ExceptionSink resolve_xsink;
    ReferenceHolder<QoreListNode> addrs(
        q_getaddrinfo_to_list(&resolve_xsink, host->host, port_str, host->family, 0, SOCK_STREAM), &resolve_xsink);
    if (resolve_xsink || !addrs || addrs->empty()) {
        bson_set_error(error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_NAME_RESOLUTION,
            "Failed to resolve '%s': %s", host->host,
            resolve_xsink ? qore_mongo_exception_desc(resolve_xsink, "name resolution failed")
                : "name resolution returned no addresses");
        if (resolve_xsink) {
            resolve_xsink.clear();
        }
        return nullptr;
    }

    // Try each address until we successfully connect
    mongoc_socket_t* sock = nullptr;
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
            if (!smh->checkNetworkAccess(sa, addrlen, IPPROTO_TCP, &xsink)) {
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

        sock = mongoc_socket_new(sa->sa_family, SOCK_STREAM, IPPROTO_TCP);
        if (!sock) {
            continue;
        }

        // Get connect timeout from URI (default 10 seconds)
        int32_t connecttimeoutms = mongoc_uri_get_option_as_int32(uri, MONGOC_URI_CONNECTTIMEOUTMS, 10000);
        int64_t expire_at = bson_get_monotonic_time() + (connecttimeoutms * 1000);

        if (mongoc_socket_connect(sock, sa, addrlen, expire_at) == 0) {
            break;  // Success
        }

        mongoc_socket_destroy(sock);
        sock = nullptr;
    }

    if (!sock) {
        bson_set_error(error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_CONNECT,
            "Failed to connect to '%s:%u'", host->host, host->port);
        return nullptr;
    }

    // Create socket stream
    mongoc_stream_t* base = mongoc_stream_socket_new(sock);
    if (!base) {
        mongoc_socket_destroy(sock);
        bson_set_error(error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_SOCKET,
            "Failed to create stream for '%s:%u'", host->host, host->port);
        return nullptr;
    }

    // Check if SSL/TLS is required
    if (mongoc_uri_get_tls(uri)) {
        // Check for interrupt/cancel before TLS setup
        if (qore_check_cancel(nullptr, "MongoDB TLS setup")) {
            mongoc_stream_destroy(base);
            bson_set_error(error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_CONNECT,
                "MongoDB TLS setup interrupted");
            return nullptr;
        }

        // Use default SSL options (nullptr means use system defaults)
        mongoc_stream_t* tls_stream = mongoc_stream_tls_new_with_hostname(base, host->host, nullptr, 1);
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

    // Wrap it with our interruptible stream
    return qore_stream_new(base);
}

void qore_mongo_setup_interruptible_streams(mongoc_client_t* client) {
    qore_mongo_set_log_handler();
    mongoc_client_set_stream_initiator(client, qore_mongo_stream_initiator, nullptr);
}

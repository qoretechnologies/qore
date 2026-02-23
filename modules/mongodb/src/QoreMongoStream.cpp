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
    struct addrinfo hints;
    struct addrinfo* result = nullptr;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = host->family;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", host->port);

    int rv = getaddrinfo(host->host, port_str, &hints, &result);
    if (rv != 0) {
        bson_set_error(error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_NAME_RESOLUTION,
            "Failed to resolve '%s': %s", host->host, gai_strerror(rv));
        return nullptr;
    }

    // Try each address until we successfully connect
    mongoc_socket_t* sock = nullptr;
    struct addrinfo* rp;

    for (rp = result; rp != nullptr; rp = rp->ai_next) {
        // Check for interrupt/cancel before each connection attempt
        if (qore_check_cancel(nullptr, "MongoDB connection")) {
            freeaddrinfo(result);
            bson_set_error(error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_CONNECT,
                "MongoDB connection interrupted");
            return nullptr;
        }

        // Check network access if sandbox manager is present
        if (smh) {
            ExceptionSink xsink;
            if (!smh->checkNetworkAccess(rp->ai_addr, rp->ai_addrlen, IPPROTO_TCP, &xsink)) {
                // Network access denied by sandbox
                if (xsink) {
                    freeaddrinfo(result);
                    bson_set_error(error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_CONNECT,
                        "MongoDB connection denied by sandbox: network access restricted");
                    return nullptr;
                }
                // Try next address
                continue;
            }
        }

        sock = mongoc_socket_new(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (!sock) {
            continue;
        }

        // Get connect timeout from URI (default 10 seconds)
        int32_t connecttimeoutms = mongoc_uri_get_option_as_int32(uri, MONGOC_URI_CONNECTTIMEOUTMS, 10000);
        int64_t expire_at = bson_get_monotonic_time() + (connecttimeoutms * 1000);

        if (mongoc_socket_connect(sock, rp->ai_addr, (mongoc_socklen_t)rp->ai_addrlen, expire_at) == 0) {
            break;  // Success
        }

        mongoc_socket_destroy(sock);
        sock = nullptr;
    }

    freeaddrinfo(result);

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

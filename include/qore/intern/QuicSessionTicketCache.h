/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QuicSessionTicketCache.h

    Qore Programming Language

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

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_INTERN_QUICSESSIONTICKETCACHE_H
#define _QORE_INTERN_QUICSESSIONTICKETCACHE_H

#include <openssl/ssl.h>

#include <cstdint>
#include <ctime>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

//! Cached TLS 1.3 session ticket with QUIC 0-RTT transport parameters
struct QuicCachedTicket {
    SSL_SESSION* session = nullptr;         //!< ref-counted by OpenSSL
    std::vector<uint8_t> transport_params;  //!< encoded 0-RTT transport params from ngtcp2
    int64_t expiry_epoch = 0;               //!< time(nullptr) + SSL_SESSION_get_timeout()

    DLLLOCAL QuicCachedTicket() = default;

    //! Move constructor
    DLLLOCAL QuicCachedTicket(QuicCachedTicket&& other) noexcept
        : session(other.session)
        , transport_params(std::move(other.transport_params))
        , expiry_epoch(other.expiry_epoch) {
        other.session = nullptr;
    }

    //! Move assignment
    DLLLOCAL QuicCachedTicket& operator=(QuicCachedTicket&& other) noexcept {
        if (this != &other) {
            if (session) {
                SSL_SESSION_free(session);
            }
            session = other.session;
            transport_params = std::move(other.transport_params);
            expiry_epoch = other.expiry_epoch;
            other.session = nullptr;
        }
        return *this;
    }

    //! Destructor: release SSL_SESSION reference
    DLLLOCAL ~QuicCachedTicket() {
        if (session) {
            SSL_SESSION_free(session);
            session = nullptr;
        }
    }

    // Non-copyable (SSL_SESSION ref management)
    QuicCachedTicket(const QuicCachedTicket&) = delete;
    QuicCachedTicket& operator=(const QuicCachedTicket&) = delete;
};

//! Thread-safe global singleton cache for QUIC 0-RTT session tickets
/** Keyed by origin ("host:port"), max MAX_CACHE_SIZE entries with TTL eviction.
    Global singleton maximizes cache hit rate — different HTTPClient instances
    connecting to the same origin share tickets.
*/
class QuicSessionTicketCache {
public:
    //! Maximum number of cached tickets
    static constexpr size_t MAX_CACHE_SIZE = 1024;

    //! Get the global singleton instance
    DLLLOCAL static QuicSessionTicketCache& instance();

    //! Store a session ticket for an origin
    /** Replaces any existing ticket for the same origin. Computes expiry from
        SSL_SESSION_get_timeout(). Opportunistically evicts expired entries.
        @param origin "host:port" key
        @param ticket the cached ticket (moved into the cache)
    */
    DLLLOCAL void store(const std::string& origin, QuicCachedTicket&& ticket);

    //! Look up a session ticket for an origin
    /** Returns a copy of the SSL_SESSION (with SSL_SESSION_up_ref()) and
        transport params. Returns false if expired or missing.
        @param origin "host:port" key
        @param session_out output: SSL_SESSION with incremented ref count (caller must free)
        @param tp_out output: copy of encoded transport params
        @return true if found and valid, false if expired or missing
    */
    DLLLOCAL bool lookup(const std::string& origin, SSL_SESSION** session_out,
                         std::vector<uint8_t>& tp_out);

    //! Remove a cached ticket for an origin (e.g., on 0-RTT rejection)
    DLLLOCAL void remove(const std::string& origin);

    //! Get the number of cached entries (for testing)
    DLLLOCAL size_t size() const;

    //! Clear all cached entries (call before OpenSSL cleanup to avoid use-after-free)
    DLLLOCAL void clear();

private:
    DLLLOCAL QuicSessionTicketCache() = default;

    // Non-copyable, non-movable
    QuicSessionTicketCache(const QuicSessionTicketCache&) = delete;
    QuicSessionTicketCache& operator=(const QuicSessionTicketCache&) = delete;

    //! Evict all expired entries (caller must hold mtx_)
    DLLLOCAL void evictExpired();

    mutable std::mutex mtx_;
    std::unordered_map<std::string, QuicCachedTicket> cache_;
};

#endif // _QORE_INTERN_QUICSESSIONTICKETCACHE_H

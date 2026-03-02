/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QuicSessionTicketCache.cpp

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

#include <qore/Qore.h>

#include "qore/intern/QuicSessionTicketCache.h"

QuicSessionTicketCache& QuicSessionTicketCache::instance() {
    static QuicSessionTicketCache cache;
    return cache;
}

void QuicSessionTicketCache::store(const std::string& origin, QuicCachedTicket&& ticket) {
    std::lock_guard<std::mutex> lock(mtx_);

    // Opportunistically evict expired entries when approaching capacity
    if (cache_.size() >= MAX_CACHE_SIZE) {
        evictExpired();
    }

    // If still at capacity after eviction, remove the oldest entry (LRU approximation)
    if (cache_.size() >= MAX_CACHE_SIZE) {
        int64_t oldest_expiry = INT64_MAX;
        std::string oldest_key;
        for (const auto& entry : cache_) {
            if (entry.second.expiry_epoch < oldest_expiry) {
                oldest_expiry = entry.second.expiry_epoch;
                oldest_key = entry.first;
            }
        }
        if (!oldest_key.empty()) {
            cache_.erase(oldest_key);
        }
    }

    // Compute expiry from session timeout
    if (ticket.session) {
        long timeout = SSL_SESSION_get_timeout(ticket.session);
        ticket.expiry_epoch = static_cast<int64_t>(time(nullptr)) + timeout;
    }

    // Store (replaces any existing entry for this origin via move assignment)
    cache_[origin] = std::move(ticket);

    printd(5, "QuicSessionTicketCache::store(): cached ticket for '%s' (expiry in %llds, %zu entries)\n",
        origin.c_str(),
        (long long)(cache_[origin].expiry_epoch - time(nullptr)),
        cache_.size());
}

bool QuicSessionTicketCache::lookup(const std::string& origin, SSL_SESSION** session_out,
                                     std::vector<uint8_t>& tp_out) {
    std::lock_guard<std::mutex> lock(mtx_);

    auto it = cache_.find(origin);
    if (it == cache_.end()) {
        return false;
    }

    // Check expiry
    if (it->second.expiry_epoch <= static_cast<int64_t>(time(nullptr))) {
        printd(5, "QuicSessionTicketCache::lookup(): expired ticket for '%s'\n", origin.c_str());
        cache_.erase(it);
        return false;
    }

    if (!it->second.session) {
        cache_.erase(it);
        return false;
    }

    // Copy transport params first (can throw std::bad_alloc), then up_ref the
    // session (cannot fail). This order ensures no SSL_SESSION ref count leak
    // if the vector copy throws.
    tp_out = it->second.transport_params;
    SSL_SESSION_up_ref(it->second.session);
    *session_out = it->second.session;

    printd(5, "QuicSessionTicketCache::lookup(): found ticket for '%s' (tp_size=%zu)\n",
        origin.c_str(), tp_out.size());

    return true;
}

void QuicSessionTicketCache::remove(const std::string& origin) {
    std::lock_guard<std::mutex> lock(mtx_);
    cache_.erase(origin);
    printd(5, "QuicSessionTicketCache::remove(): removed ticket for '%s'\n", origin.c_str());
}

void QuicSessionTicketCache::evictExpired() {
    // NOTE: caller must hold mtx_
    int64_t now = static_cast<int64_t>(time(nullptr));
    auto it = cache_.begin();
    while (it != cache_.end()) {
        if (it->second.expiry_epoch <= now) {
            printd(5, "QuicSessionTicketCache::evictExpired(): evicting '%s'\n", it->first.c_str());
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

size_t QuicSessionTicketCache::size() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return cache_.size();
}

void QuicSessionTicketCache::clear() {
    std::lock_guard<std::mutex> lock(mtx_);
    cache_.clear();
}

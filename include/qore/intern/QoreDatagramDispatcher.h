/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreDatagramDispatcher.h

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

#ifndef _QORE_DATAGRAMDISPATCHER_H
#define _QORE_DATAGRAMDISPATCHER_H

#include <qore/Qore.h>

#include <ngtcp2/ngtcp2.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>

class QoreSSLPrivateKey;

//! Connection-ID based datagram dispatcher for QUIC preparation
/** This class provides connection-ID demultiplexing for UDP datagrams.
    Multiple QUIC connections share a single UDP socket; incoming datagrams
    are dispatched to the appropriate connection handler based on the
    connection ID extracted from the packet header.

    Thread safety: all methods are thread-safe (internal locking).

    @since %Qore 3.0
*/
class QoreDatagramDispatcher {
public:
    DLLLOCAL QoreDatagramDispatcher() = default;
    DLLLOCAL ~QoreDatagramDispatcher() = default;

    //! Register a connection ID with a session
    /** @param cid the connection ID as a binary string
        @param session_id the QUIC session ID of the owning session

        The map stores session IDs, never pointers: a dispatch hit must be
        resolved (pinned) through an owning session map by the caller, so a
        stale CID entry for a session that has already been removed can only
        produce a lookup miss — never a dangling pointer.
    */
    DLLLOCAL void registerConnectionId(const std::string& cid, int64_t session_id) {
        AutoLocker al(lock);
        cid_map[cid] = session_id;
    }

    //! Unregister a connection ID
    /** @param cid the connection ID to remove
    */
    DLLLOCAL void unregisterConnectionId(const std::string& cid) {
        AutoLocker al(lock);
        cid_map.erase(cid);
    }

    //! Look up the session ID for a connection ID
    /** @param cid the connection ID to look up
        @return the session ID, or -1 if not found
    */
    DLLLOCAL int64_t lookup(const std::string& cid) const {
        AutoLocker al(lock);
        auto it = cid_map.find(cid);
        return it != cid_map.end() ? it->second : -1;
    }

    //! Dispatch a datagram to the appropriate session
    /** Extracts the connection ID from the datagram and looks up the session ID.

        @param data the datagram data
        @param len the datagram length
        @return the session ID, or -1 if the CID is not registered.  The caller
                must pin the session by looking the ID up in an owning map
                (SocketQuicServerPollOperation::sessions_ or
                qore_socket_private::getQuicSession()) and drop the packet on a
                miss; the ID may refer to a session that has since been removed,
                which a map lookup detects safely — unlike the raw pointer this
                method used to return, which could dangle when the session's
                last reference was dropped on another thread after removal.
    */
    DLLLOCAL int64_t dispatch(const uint8_t* data, size_t len) {
        if (len < 1) {
            return -1;
        }

        ngtcp2_version_cid vc;
        int rv = ngtcp2_pkt_decode_version_cid(&vc, data, len, NGTCP2_MAX_CIDLEN);
        if (rv != 0) {
            return -1;  // not a valid QUIC packet
        }

        // Extract DCID (destination connection ID) for lookup.
        // NGTCP2_MAX_CIDLEN is 20 bytes; most stdlib SSO covers strings up to
        // ~22 bytes, so this typically avoids heap allocation.
        std::string dcid(reinterpret_cast<const char*>(vc.dcid), vc.dcidlen);

        AutoLocker al(lock);
        auto it = cid_map.find(dcid);
        if (it != cid_map.end()) {
            return it->second;
        }

        return -1;
    }

    //! Returns the number of registered connection IDs
    DLLLOCAL size_t size() const {
        AutoLocker al(lock);
        return cid_map.size();
    }

    //! Returns true if no connection IDs are registered
    DLLLOCAL bool empty() const {
        AutoLocker al(lock);
        return cid_map.empty();
    }

    //! Clear all registered connection IDs
    DLLLOCAL void clear() {
        AutoLocker al(lock);
        cid_map.clear();
    }

    //! The length of the static key of the listener's stateless resets
    static constexpr size_t StatelessResetKeyLen = 32;
    //! The length of the connection IDs that a server issues, and so of the DCID of a short-header packet sent to it
    static constexpr size_t ServerCidLen = NGTCP2_MAX_CIDLEN;
    //! The most stateless resets that the listener sends in a burst (RFC 9000 section 10.3)
    static constexpr int StatelessResetBurst = 64;
    //! The time in which the listener can send one more stateless reset, up to the burst, in nanoseconds
    static constexpr uint64_t StatelessResetRefillNs = 10000000;
    //! The most random bytes of a stateless reset: one of the size of a short-header packet with the longest
    //! connection ID and the smallest payload (as ngtcp2's own server)
    static constexpr size_t StatelessResetMaxRandLen = NGTCP2_MAX_CIDLEN + 22 - NGTCP2_STATELESS_RESET_TOKENLEN;

    //! Derives the static key of the listener's stateless resets from the listener's private key, once (qore#6931)
    /** The stateless reset tokens of the server's connections are derived from this key (RFC 9000 section 10.3.2),
        not from a random secret of each connection, so that a server that restarts with the same private key resets
        the connections of the server before it at once.  The key is derived with HKDF-SHA256 under a label of its
        own, so that a token neither reveals nor reuses the private key.  Thread-safe.

        @param pk the listener's private key
        @return true if the dispatcher has the key; false if it could not be derived (the tokens are then random, and
        no stateless reset is sent)
    */
    DLLLOCAL bool ensureStatelessResetKey(QoreSSLPrivateKey* pk);

    //! Returns true if the dispatcher has the static key of the listener's stateless resets
    DLLLOCAL bool hasStatelessResetKey() const {
        return sr_key_set.load(std::memory_order_acquire);
    }

    //! Writes the stateless reset token of a connection ID of the server, derived from the static key
    /** @param cid the connection ID
        @param cidlen its length
        @param token receives NGTCP2_STATELESS_RESET_TOKENLEN bytes

        @return true if the token was written; false if the dispatcher has no key or the token could not be made
    */
    DLLLOCAL bool makeStatelessResetToken(const uint8_t* cid, size_t cidlen, uint8_t* token) const;

    //! Writes the stateless reset that answers a datagram that no connection of the listener takes (RFC 9000 10.3)
    /** A reset is made only for a short-header packet, which has a connection ID of the server's length; it is
        smaller than the packet (RFC 9000 section 10.3.3), so two endpoints cannot answer each other's resets for
        ever, and the listener sends at most @ref StatelessResetBurst of them in a burst, and one more each
        @ref StatelessResetRefillNs.

        @param pkt the datagram received
        @param pktlen its length
        @param out receives the reset
        @param outlen the size of \a out
        @param now_ns the time on the monotonic clock, in nanoseconds, for the rate limit

        @return the length of the reset written; 0 if none is to be sent
    */
    DLLLOCAL size_t makeStatelessReset(const uint8_t* pkt, size_t pktlen, uint8_t* out, size_t outlen,
        uint64_t now_ns);

    //! Returns the length of the stateless reset that answers a short-header packet of the given length, 0 if none
    /** The reset is smaller than the packet and has at least NGTCP2_MIN_STATELESS_RESET_RANDLEN random bytes
    */
    DLLLOCAL static size_t statelessResetRandLen(size_t pktlen) {
        if (pktlen < 1 + ServerCidLen || pktlen < 1 + NGTCP2_STATELESS_RESET_TOKENLEN
                + NGTCP2_MIN_STATELESS_RESET_RANDLEN) {
            return 0;
        }
        // the reset (the random bytes and the token) is at least one byte smaller than the packet
        size_t randlen = pktlen - 1 - NGTCP2_STATELESS_RESET_TOKENLEN;
        return randlen > StatelessResetMaxRandLen ? StatelessResetMaxRandLen : randlen;
    }

private:
    std::unordered_map<std::string, int64_t> cid_map;
    mutable QoreThreadLock lock;

    //! the static key of the listener's stateless resets; written once, before \c sr_key_set is set
    uint8_t sr_key[StatelessResetKeyLen] = {};
    //! true once \c sr_key is set
    std::atomic<bool> sr_key_set{false};
    //! true if the key could not be derived: it is not tried again
    bool sr_key_failed = false;
    //! the stateless resets that the listener can still send in the current burst (guarded by \c lock)
    int sr_bucket = StatelessResetBurst;
    //! the time of the last refill of \c sr_bucket on the monotonic clock, in nanoseconds (guarded by \c lock)
    uint64_t sr_last_refill_ns = 0;
};

#endif // _QORE_DATAGRAMDISPATCHER_H

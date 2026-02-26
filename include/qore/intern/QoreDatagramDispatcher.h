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

#include <cstring>
#include <string>
#include <unordered_map>

//! Connection-ID based datagram dispatcher for QUIC preparation
/** This class provides connection-ID demultiplexing for UDP datagrams.
    Multiple QUIC connections share a single UDP socket; incoming datagrams
    are dispatched to the appropriate connection handler based on the
    connection ID extracted from the packet header.

    Thread safety: all methods are thread-safe (internal locking).

    @since %Qore 2.3
*/
class QoreDatagramDispatcher {
public:
    DLLLOCAL QoreDatagramDispatcher() = default;
    DLLLOCAL ~QoreDatagramDispatcher() = default;

    //! Register a connection ID with a handler
    /** @param cid the connection ID as a binary string
        @param handler opaque pointer to the connection handler
    */
    DLLLOCAL void registerConnectionId(const std::string& cid, void* handler) {
        AutoLocker al(lock);
        cid_map[cid] = handler;
    }

    //! Unregister a connection ID
    /** @param cid the connection ID to remove
    */
    DLLLOCAL void unregisterConnectionId(const std::string& cid) {
        AutoLocker al(lock);
        cid_map.erase(cid);
    }

    //! Look up handler for a connection ID
    /** @param cid the connection ID to look up
        @return the handler pointer, or nullptr if not found
    */
    DLLLOCAL void* lookup(const std::string& cid) const {
        AutoLocker al(lock);
        auto it = cid_map.find(cid);
        return it != cid_map.end() ? it->second : nullptr;
    }

    //! Dispatch a datagram to the appropriate handler
    /** Extracts the connection ID from the datagram and looks up the handler.

        @param data the datagram data
        @param len the datagram length
        @return the handler pointer (a QuicSession* whose shared_ptr is held
                in qore_socket_private::quic_sessions), or nullptr if the CID
                is not registered.  The returned pointer is valid only while the
                caller holds priv->m, which protects session removal.
    */
    DLLLOCAL void* dispatch(const uint8_t* data, size_t len) {
        if (len < 1) {
            return nullptr;
        }

        ngtcp2_version_cid vc;
        int rv = ngtcp2_pkt_decode_version_cid(&vc, data, len, NGTCP2_MAX_CIDLEN);
        if (rv != 0) {
            return nullptr;  // not a valid QUIC packet
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

        return nullptr;
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

private:
    std::unordered_map<std::string, void*> cid_map;
    mutable QoreThreadLock lock;
};

#endif // _QORE_DATAGRAMDISPATCHER_H

/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QuicCommon.h

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

#ifndef _QORE_QUICCOMMON_H
#define _QORE_QUICCOMMON_H

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

//! Maximum packets per sendmmsg() batch for QUIC I/O
constexpr int QUIC_COMMON_MAX_SEND_BATCH = 64;

//! Maximum QUIC packet length (UDP payload)
constexpr size_t QUIC_COMMON_MAX_PKTLEN = 1500;

//! Flat packet buffer for batched QUIC packet I/O
/** Eliminates per-packet heap allocations by storing all packets in a single
    contiguous buffer.  The buffer and offset vector grow as needed and are
    reused across calls.  After warmup, writePackets() + sendQuicPacketsBatch()
    require zero heap allocations per poll cycle.

    @since %Qore 2.3
*/
struct QuicPacketBatch {
    //! Flat buffer holding all packet data end-to-end
    std::vector<uint8_t> buf;
    //! Per-packet start offsets and lengths (grows dynamically)
    std::vector<uint32_t> pkt_offsets;
    std::vector<uint16_t> pkt_lengths;

    //! Reserve typical capacity on construction (avoids first-use realloc)
    DLLLOCAL QuicPacketBatch() {
        buf.reserve(8 * QUIC_COMMON_MAX_PKTLEN);
        pkt_offsets.reserve(QUIC_COMMON_MAX_SEND_BATCH);
        pkt_lengths.reserve(QUIC_COMMON_MAX_SEND_BATCH);
    }

    //! Append a packet to the batch
    DLLLOCAL void addPacket(const uint8_t* data, size_t len) {
        assert(len <= QUIC_COMMON_MAX_PKTLEN);
        // Clamp to max packet length in release builds (assert stripped by -DNDEBUG)
        if (len > QUIC_COMMON_MAX_PKTLEN) {
            printd(0, "QuicPacketBatch::addPacket() WARNING: packet truncated from %zu to %zu bytes\n",
                len, QUIC_COMMON_MAX_PKTLEN);
            len = QUIC_COMMON_MAX_PKTLEN;
        }
        pkt_offsets.push_back(static_cast<uint32_t>(buf.size()));
        pkt_lengths.push_back(static_cast<uint16_t>(len));
        buf.insert(buf.end(), data, data + len);
    }

    //! Get pointer to packet data by index
    DLLLOCAL const uint8_t* packetData(int i) const {
        assert(i >= 0 && i < static_cast<int>(pkt_offsets.size()));
        return buf.data() + pkt_offsets[i];
    }
    //! Get packet length by index
    DLLLOCAL size_t packetLen(int i) const {
        assert(i >= 0 && i < static_cast<int>(pkt_lengths.size()));
        return pkt_lengths[i];
    }

    //! Reset for reuse (keeps allocated memory)
    DLLLOCAL void clear() {
        buf.clear();
        pkt_offsets.clear();
        pkt_lengths.clear();
    }

    //! Remove the first @a count packets (already sent)
    /** Compacts the buffer in-place.  O(remaining_data) but only called
        on partial sends, which are rare under normal operation.
    */
    DLLLOCAL void removeFront(int count) {
        assert(count >= 0 && count <= size());
        if (count == 0) {
            return;
        }
        if (count >= size()) {
            clear();
            return;
        }
        // Byte offset where unsent data starts
        uint32_t byte_start = pkt_offsets[count];
        size_t remaining_bytes = buf.size() - byte_start;
        if (remaining_bytes > 0) {
            memmove(buf.data(), buf.data() + byte_start, remaining_bytes);
        }
        buf.resize(remaining_bytes);
        // Shift offset/length arrays
        int remaining = size() - count;
        for (int i = 0; i < remaining; ++i) {
            pkt_offsets[i] = pkt_offsets[i + count] - byte_start;
            pkt_lengths[i] = pkt_lengths[i + count];
        }
        pkt_offsets.resize(remaining);
        pkt_lengths.resize(remaining);
    }

    //! Returns true if no packets are buffered
    DLLLOCAL bool empty() const { return pkt_offsets.empty(); }
    //! Returns the number of packets in the batch
    DLLLOCAL int size() const { return static_cast<int>(pkt_offsets.size()); }
};

//! Batch-send UDP datagrams from a QuicPacketBatch
/** Uses sendmmsg() on Linux for efficiency; falls back to sendto() loop
    on macOS/FreeBSD.  Uses stack-allocated mmsghdr/iovec arrays (no heap
    allocations) since batch size is bounded by QUIC_COMMON_MAX_SEND_BATCH.
    @param fd socket file descriptor
    @param batch the packet batch to send
    @param addr destination address, or nullptr for connected UDP sockets
    @param addrlen destination address length, or 0 for connected UDP sockets
    @return number of messages sent, or -1 on error (errno set)

    @since %Qore 2.3
*/
inline int sendQuicPacketsBatch(int fd, const QuicPacketBatch& batch,
                                       const struct sockaddr* addr, socklen_t addrlen) {
    int total = batch.size();
    int offset = 0;
#ifdef __linux__
    while (offset < total) {
        int batch_size = std::min(total - offset, QUIC_COMMON_MAX_SEND_BATCH);
        // Stack-allocated: batch_size <= QUIC_COMMON_MAX_SEND_BATCH (64)
        // ~5KB total on stack (64 * 56 + 64 * 16)
        struct mmsghdr msgs[QUIC_COMMON_MAX_SEND_BATCH];
        struct iovec iovecs[QUIC_COMMON_MAX_SEND_BATCH];
        memset(msgs, 0, sizeof(struct mmsghdr) * batch_size);

        for (int i = 0; i < batch_size; ++i) {
            iovecs[i].iov_base = const_cast<uint8_t*>(batch.packetData(offset + i));
            iovecs[i].iov_len = batch.packetLen(offset + i);
            msgs[i].msg_hdr.msg_name = const_cast<struct sockaddr*>(addr);
            msgs[i].msg_hdr.msg_namelen = addrlen;
            msgs[i].msg_hdr.msg_iov = &iovecs[i];
            msgs[i].msg_hdr.msg_iovlen = 1;
        }

        int sent;
        do {
            sent = sendmmsg(fd, msgs, static_cast<unsigned int>(batch_size), 0);
        } while (sent < 0 && errno == EINTR);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (offset > 0) {
                    break;  // partial send — return what we sent
                }
                return -1;  // nothing sent
            }
            // If prior batches succeeded, return partial count instead of error
            if (offset > 0) {
                break;
            }
            return -1;
        }
        offset += sent;
        if (sent < batch_size) {
            break;  // partial send — stop batching
        }
    }
#else
    // macOS/FreeBSD: sendto() loop fallback
    while (offset < total) {
        ssize_t sent;
        do {
            sent = sendto(fd, batch.packetData(offset), batch.packetLen(offset), 0, addr, addrlen);
        } while (sent < 0 && errno == EINTR);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (offset > 0) {
                    break;  // partial send — return what we sent
                }
                return -1;  // nothing sent
            }
            if (offset > 0) {
                break;  // partial send — return what we sent
            }
            return -1;
        }
        ++offset;
    }
#endif
    // Partial sends are expected under congestion; QUIC retransmission handles recovery.
    // Callers should check: return value < batch.size() means some packets were not sent.
    return offset;
}

//! Enable per-packet destination address reporting (IP_PKTINFO / IPV6_PKTINFO)
/** Required for correct QUIC path validation on multi-homed hosts where the
    socket is bound to a wildcard address (0.0.0.0/::). Without this,
    getsockname() returns the wildcard, but pktinfo provides the actual
    destination IP for each received packet.

    @param fd socket file descriptor
    @param family address family (AF_INET or AF_INET6)
    @return 0 on success, -1 on failure (errno set)

    @since %Qore 2.3
*/
inline int enableQuicPktinfo(int fd, int family) {
    int val = 1;
    if (family == AF_INET) {
#ifdef IP_PKTINFO
        return setsockopt(fd, IPPROTO_IP, IP_PKTINFO, &val, sizeof(val));
#elif defined(IP_RECVDSTADDR)
        return setsockopt(fd, IPPROTO_IP, IP_RECVDSTADDR, &val, sizeof(val));
#else
        return -1;
#endif
    } else if (family == AF_INET6) {
#ifdef IPV6_RECVPKTINFO
        return setsockopt(fd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &val, sizeof(val));
#else
        return -1;
#endif
    }
    return -1;
}

//! Maximum size of ancillary data buffer for recvmsg() pktinfo extraction
constexpr size_t QUIC_CMSG_BUF_SIZE = 128;

//! Receive a single UDP datagram with ancillary data (control messages)
/** Wraps recvmsg() to provide both the source address and control message
    data (for IP_PKTINFO extraction). On return, src_addrlen and cmsg_len
    are updated to reflect actual sizes.

    @param fd socket file descriptor
    @param buf buffer for packet data
    @param buflen buffer size
    @param flags recvmsg flags
    @param src_addr [out] source address
    @param src_addrlen [in/out] source address length
    @param cmsg_buf [out] control message buffer
    @param cmsg_len [in/out] control message buffer length
    @return bytes read, or -1 on error (errno set)

    @since %Qore 2.3
*/
inline ssize_t recvQuicPacket(int fd, void* buf, size_t buflen, int flags,
                              struct sockaddr* src_addr, socklen_t* src_addrlen,
                              void* cmsg_buf, size_t* cmsg_len) {
    struct iovec iov;
    iov.iov_base = buf;
    iov.iov_len = buflen;

    struct msghdr msg{};
    msg.msg_name = src_addr;
    msg.msg_namelen = *src_addrlen;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = *cmsg_len;

    ssize_t nread;
    do {
        nread = recvmsg(fd, &msg, flags);
    } while (nread < 0 && errno == EINTR);
    if (nread >= 0) {
        *src_addrlen = msg.msg_namelen;
        *cmsg_len = msg.msg_controllen;
        // Discard truncated datagrams — ngtcp2 would reject a partial QUIC packet anyway.
        // Use EMSGSIZE (not EAGAIN) so callers can distinguish "truncated" from "no data"
        // and continue draining valid subsequent packets from the socket buffer.
        if (msg.msg_flags & MSG_TRUNC) {
            errno = EMSGSIZE;
            return -1;
        }
    }
    return nread;
}

//! Extract the per-packet destination IP from recvmsg() control messages
/** Overwrites only the address field (not port or family) in dest_addr.
    Uses ipi_spec_dst (Linux) or IP_RECVDSTADDR (macOS/FreeBSD) for IPv4,
    and IPV6_PKTINFO for IPv6.

    @param cmsg_buf control message buffer from recvmsg()
    @param cmsg_len control message data length
    @param family address family (AF_INET or AF_INET6)
    @param dest_addr [in/out] destination address — IP overwritten on success
    @return true if address was extracted, false if not found (dest_addr unchanged)

    @since %Qore 2.3
*/
inline bool extractPktinfoAddr(const void* cmsg_buf, size_t cmsg_len,
                               int family, struct sockaddr_storage* dest_addr) {
    // Reconstruct a temporary msghdr to iterate control messages
    struct msghdr msg{};
    msg.msg_control = const_cast<void*>(cmsg_buf);
    msg.msg_controllen = cmsg_len;

    for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
#ifdef IP_PKTINFO
        if (family == AF_INET && cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO) {
            auto* pi = reinterpret_cast<struct in_pktinfo*>(CMSG_DATA(cmsg));
            auto* sin = reinterpret_cast<struct sockaddr_in*>(dest_addr);
            sin->sin_addr = pi->ipi_spec_dst;
            return true;
        }
#elif defined(IP_RECVDSTADDR)
        if (family == AF_INET && cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_RECVDSTADDR) {
            auto* in_addr_p = reinterpret_cast<struct in_addr*>(CMSG_DATA(cmsg));
            auto* sin = reinterpret_cast<struct sockaddr_in*>(dest_addr);
            sin->sin_addr = *in_addr_p;
            return true;
        }
#endif
        if (family == AF_INET6 && cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO) {
            auto* pi6 = reinterpret_cast<struct in6_pktinfo*>(CMSG_DATA(cmsg));
            auto* sin6 = reinterpret_cast<struct sockaddr_in6*>(dest_addr);
            sin6->sin6_addr = pi6->ipi6_addr;
            return true;
        }
    }
    return false;
}

//! HTTP/1.x hop-by-hop headers forbidden in HTTP/2 and HTTP/3
/** RFC 9113 Section 8.2.2 (HTTP/2), RFC 9114 Section 4.2 (HTTP/3).
    Shared between Http2Session.cpp and QuicSession.cpp.

    @since %Qore 2.3
*/
inline const std::unordered_set<std::string>& getForbiddenHopByHopHeaders() {
    static const std::unordered_set<std::string> headers = {
        "connection", "keep-alive", "proxy-connection", "transfer-encoding", "upgrade"
    };
    return headers;
}

#endif // _QORE_QUICCOMMON_H

/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QuicPollOperations.cpp

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
#include <qore/QoreAbstractLoggerInterface.h>

#include "qore/intern/QC_SocketPollOperation.h"
#include "qore/intern/QoreAsyncIoLogger.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

// Maximum packets per recvmmsg() batch
constexpr int QUIC_MAX_RECV_BATCH = 16;
// QUIC_RECV_BUF_SIZE is defined in QC_SocketPollOperation.h

// Shared sendmmsg() batch helper for QUIC I/O
#include "qore/intern/QuicCommon.h"

//! Maximum poll timeout (ms) when no QUIC timer is pending.
/** Fallback poll timeout (ms) when ngtcp2 reports no timer pending
    (ngtcp2_conn_get_expiry() == UINT64_MAX).  Per RFC 9002, ACK-only packets
    are not loss-detected, so the peer's retransmission timer drives recovery.
    1 second is a reasonable fallback for this rare edge case.
*/
static constexpr int64_t QUIC_NO_TIMER_POLL_MS = 1000;

//! Maximum packets to process per continuePoll() call for QUIC client connections.
/** Prevents a single connection from monopolizing the shared I/O thread when
    multiple QUIC connections are active.  At ~1μs/packet, a budget of 32 gives
    ~32μs per continuePoll() call, ensuring fair scheduling across connections.
    When the budget is exhausted and more packets remain, continuePoll() returns
    poll_info with poll_timeout_ms=0 to trigger an immediate re-poll.
*/
static constexpr int QUIC_CLIENT_RECV_BUDGET = 32;

//! Grace period (nanoseconds) after the first ICMP "port unreachable" during
//! a QUIC client handshake before failing the operation with
//! QUIC-CONNECT-REFUSED.  Linux rate-limits destination-unreachable ICMPs
//! (~1 per second per peer), so a "consecutive count" approach can't tell a
//! permanently closed UDP port from a transient bind race — only the first
//! refusal reliably surfaces.  This grace window lets ngtcp2's PTO retransmits
//! succeed if the peer's UDP listener was just briefly missing (e.g., a server
//! restart that races with our Initial), while still failing fast on a closed
//! port instead of burning the entire connect_timeout.  1.5 s comfortably
//! covers a couple of PTO retransmits at the typical 333 ms initial RTT.
static constexpr int64_t QUIC_ECONNREFUSED_GRACE_NS = 1500000000LL;

static int quic_input_stream_pollable_descriptor(InputStream* is, const char* err, ExceptionSink* xsink) {
    if (!is->supportsNonBlockingIo()) {
        return -1;
    }

    int fd = is->getPollableDescriptor();
    if (fd < 0) {
        xsink->raiseException(err, "InputStream reports non-blocking I/O support but returned no pollable descriptor");
    }
    return fd;
}

//! Set the "poll_timeout_ms" key on a poll info hash based on the next QUIC timer expiry.
/** Translates the ngtcp2 timer expiry into a poll timeout hint so the I/O
    thread's poll() wakes when the QUIC retransmission/PTO timer fires.
    @param poll_info the poll info hash to update (must not be nullptr)
    @param expiry the next timer expiry (UINT64_MAX if no timer pending)
    @param xsink for exception handling
*/
static void setPollTimeoutFromExpiry(QoreHashNode* poll_info, ngtcp2_tstamp expiry,
                                     ExceptionSink* xsink, bool nothing_to_send = false) {
    int64_t timeout_ms;
    if (expiry == UINT64_MAX) {
        timeout_ms = QUIC_NO_TIMER_POLL_MS;
    } else {
        ngtcp2_tstamp now = QuicSession::timestamp();
        if (expiry <= now) {
            // Timer still in the past after processTimerAndWrite handled it.
            // Don't set a sub-ms timeout — the timer work was already done
            // and tight-polling won't help.  Use the default fallback so the
            // poll blocks until the next packet arrives or the fallback fires.
            // Matches curl's pattern: if expiry <= ts after handle+flush,
            // no Curl_expire is set — the next I/O event wakes the loop.
            timeout_ms = QUIC_NO_TIMER_POLL_MS;
        } else {
            ngtcp2_duration timeout_ns = expiry - now;
            timeout_ms = static_cast<int64_t>(timeout_ns / 1000000);
            // Round up sub-millisecond durations to 1ms (curl pattern: line 933-934)
            if (timeout_ns % 1000000) {
                ++timeout_ms;
            }
            // Anti-spin (issue #5351): if the timer-and-write pass produced
            // nothing to send (e.g. a stream is flow-control-blocked and we are
            // waiting for the peer's MAX_STREAM_DATA / an ACK to reopen the
            // congestion window), a sub-millisecond timer wakeup just re-runs the
            // same no-op write and busy-loops the I/O thread.  The event that
            // actually unblocks progress is an INCOMING packet, which wakes the
            // poll regardless of the timeout, so fall back to the long poll
            // instead of tight-polling — same rationale as the expiry<=now branch.
            if (nothing_to_send && timeout_ms <= 1) {
                timeout_ms = QUIC_NO_TIMER_POLL_MS;
            }
        }
    }
    poll_info->setKeyValue("poll_timeout_ms", timeout_ms, xsink);
}

//! Clamp poll_timeout_ms so the I/O thread wakes no later than deadline_ns.
/** Used when a handshake deadline is active: ngtcp2's retransmission timer may
    point further out than our connect_timeout, so we shorten the poll window to
    ensure continuePoll() re-runs in time to fail the op with QUIC-HANDSHAKE-TIMEOUT.
    @param poll_info the poll info hash (must not be nullptr)
    @param deadline_ns absolute deadline (ngtcp2 timestamp, ns); <=0 disables
    @param xsink for exception handling
*/
static void clampPollTimeoutToDeadline(QoreHashNode* poll_info, int64_t deadline_ns,
                                       ExceptionSink* xsink) {
    if (deadline_ns <= 0 || !poll_info) {
        return;
    }
    ngtcp2_tstamp now = QuicSession::timestamp();
    int64_t remaining_ms;
    if (static_cast<int64_t>(now) >= deadline_ns) {
        remaining_ms = 0;  // deadline passed; fire immediately on next poll
    } else {
        int64_t remaining_ns = deadline_ns - static_cast<int64_t>(now);
        remaining_ms = remaining_ns / 1000000;
        if (remaining_ns % 1000000) {
            ++remaining_ms;  // round up so we don't wake one tick early
        }
    }
    QoreValue ptv = poll_info->getKeyValue("poll_timeout_ms");
    if (ptv.getType() == NT_INT) {
        int64_t current_ms = ptv.getAsBigInt();
        // -1 = wait indefinitely; anything negative shouldn't happen here but
        // treat as "no bound" so we still clamp it down
        if (current_ms < 0 || remaining_ms < current_ms) {
            poll_info->setKeyValue("poll_timeout_ms", remaining_ms, xsink);
        }
    } else {
        poll_info->setKeyValue("poll_timeout_ms", remaining_ms, xsink);
    }
}

//! Log and clear a readPacketBatch exception (shared by batch recv helpers)
static void logBatchError(ExceptionSink* xsink, QuicSession* target, int batch_count) {
    QoreStringValueHelper err_str(xsink->getExceptionErr());
    QoreStringValueHelper desc_str(xsink->getExceptionDesc());
    fprintf(stderr, "QUIC WARNING: readPacketBatch error "
        "(session %lld%s, %d packets): %s: %s\n",
        (long long)target->getSessionId(),
        target->isClosed() ? ", now closed" : "",
        batch_count,
        *err_str ? err_str->c_str() : "unknown",
        *desc_str ? desc_str->c_str() : "unknown");
    xsink->clear();
}

//! Batch-receive and dispatch QUIC packets via recvmmsg (Linux) or recvmsg (other).
/** Receives all available packets from the socket and dispatches them to their
    target QuicSession via CID-based routing.  On Linux, uses recvmmsg + readPacketBatch
    for batched processing with a single lock acquisition per session.
    @param fd the UDP socket file descriptor
    @param dispatcher CID-based packet router
    @param local_addr cached local address (from getsockname)
    @param local_addrlen local address length
    @param recv_buf fallback receive buffer (used on non-Linux platforms)
    @param recv_buf_size size of recv_buf
    @param spriv the socket private data used to pin dispatched sessions: the
           dispatcher returns session IDs, and each hit must be resolved to a
           shared_ptr via spriv->getQuicSession() — a miss means the session was
           removed (reaped/aborted) and the packet is dropped, so a session
           whose destruction is pending on another thread can never be accessed
    @param xsink exception sink
    @return 0 on success, -1 on fatal error
*/
static int recvAndDispatchQuicPackets(int fd, QoreDatagramDispatcher& dispatcher,
    const struct sockaddr_storage& local_addr, socklen_t local_addrlen,
    uint8_t* recv_buf, size_t recv_buf_size, qore_socket_private* spriv,
    ExceptionSink* xsink) {
#ifdef __linux__
    // Batch receive with recvmmsg for reduced syscall overhead.
    // All large arrays are thread-local to avoid ~8.5KB of stack pressure per call
    // (mmsghdr + iovec + sockaddr_storage + cmsg + batch + pkt_locals).
    static thread_local uint8_t bufs[QUIC_MAX_RECV_BATCH][QUIC_RECV_BUF_SIZE];
    static thread_local struct mmsghdr msgs[QUIC_MAX_RECV_BATCH];
    static thread_local struct iovec iovecs[QUIC_MAX_RECV_BATCH];
    static thread_local struct sockaddr_storage addrs[QUIC_MAX_RECV_BATCH];
    static thread_local uint8_t cmsg_bufs[QUIC_MAX_RECV_BATCH][QUIC_CMSG_BUF_SIZE];
    static thread_local QuicReceivedPacket batch[QUIC_MAX_RECV_BATCH];
    static thread_local struct sockaddr_storage pkt_locals[QUIC_MAX_RECV_BATCH];
    while (true) {
        memset(msgs, 0, sizeof(msgs));

        for (int i = 0; i < QUIC_MAX_RECV_BATCH; ++i) {
            iovecs[i].iov_base = bufs[i];
            iovecs[i].iov_len = QUIC_RECV_BUF_SIZE;
            msgs[i].msg_hdr.msg_name = &addrs[i];
            msgs[i].msg_hdr.msg_namelen = sizeof(addrs[i]);
            msgs[i].msg_hdr.msg_iov = &iovecs[i];
            msgs[i].msg_hdr.msg_iovlen = 1;
            msgs[i].msg_hdr.msg_control = cmsg_bufs[i];
            msgs[i].msg_hdr.msg_controllen = QUIC_CMSG_BUF_SIZE;
        }

        int nrecv = recvmmsg(fd, msgs, QUIC_MAX_RECV_BATCH, MSG_DONTWAIT, nullptr);
        if (nrecv < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;  // no more data available
            }
            if (errno == EINTR) {
                continue;  // retry on signal interruption
            }
            xsink->raiseErrnoException("QUIC-RECV-ERROR", errno, "recvmmsg() failed");
            return -1;
        }
        if (nrecv == 0) {
            return 0;
        }

        // Batch packets by target session for readPacketBatch();
        // common case: all packets go to the same session (single client).
        // Invariant: batch_count <= nrecv <= QUIC_MAX_RECV_BATCH (packets are
        // only accumulated, never added beyond what recvmmsg returned).
        int batch_count = 0;
        QuicSession* batch_target = nullptr;
        // Pins batch_target for the lifetime of the accumulated batch
        std::shared_ptr<QuicSession> batch_target_sp;

        for (int i = 0; i < nrecv; ++i) {
            // Discard truncated datagrams — ngtcp2 would reject partial packets
            if (msgs[i].msg_hdr.msg_flags & MSG_TRUNC) {
                continue;
            }
            size_t pkt_len = msgs[i].msg_len;
            if (pkt_len == 0) {
                continue;
            }

            // Route to the correct session via dispatcher; drop packets
            // with unrecognized DCIDs rather than misrouting them.  Pin the
            // session through the owning map — a miss means the session was
            // removed and the packet must be dropped (see function docs)
            int64_t sid = dispatcher.dispatch(bufs[i], pkt_len);
            if (sid < 0) {
                continue;
            }
            std::shared_ptr<QuicSession> target_sp = spriv->getQuicSession(sid);
            if (!target_sp) {
                continue;
            }
            QuicSession* target = target_sp.get();

            // Per-packet local address from pktinfo
            assert(batch_count < QUIC_MAX_RECV_BATCH);
            memcpy(&pkt_locals[batch_count], &local_addr, local_addrlen);
            extractPktinfoAddr(cmsg_bufs[i], msgs[i].msg_hdr.msg_controllen,
                               local_addr.ss_family, &pkt_locals[batch_count]);

            if (target == batch_target || !batch_target) {
                // Same session (or first packet) — accumulate in batch
                batch_target = target;
                batch_target_sp = target_sp;
                ngtcp2_path& path = batch[batch_count].path;
                path.local.addr = reinterpret_cast<ngtcp2_sockaddr*>(&pkt_locals[batch_count]);
                path.local.addrlen = local_addrlen;
                path.remote.addr = reinterpret_cast<ngtcp2_sockaddr*>(&addrs[i]);
                path.remote.addrlen = msgs[i].msg_hdr.msg_namelen;
                batch[batch_count].data = bufs[i];
                batch[batch_count].len = pkt_len;
                ++batch_count;
            } else {
                // Different session — flush current batch first
                if (batch_count > 0) {
                    batch_target->readPacketBatch(batch, batch_count, xsink);
                    if (*xsink) {
                        logBatchError(xsink, batch_target, batch_count);
                    }
                }
                // Start new batch for the different session.
                // saved_idx captures where the current packet's pktinfo was
                // written (at pkt_locals[batch_count] above, before the
                // if/else).  After resetting batch_count to 0, we relocate
                // that entry to pkt_locals[0] so it becomes the first element
                // of the new batch.  When saved_idx == 0, source and dest
                // are already the same slot, so no copy is needed.
                int saved_idx = batch_count;
                batch_target = target;
                batch_target_sp = target_sp;
                batch_count = 0;
                if (saved_idx != 0) {
                    memcpy(&pkt_locals[0], &pkt_locals[saved_idx], sizeof(pkt_locals[0]));
                }
                ngtcp2_path& path = batch[0].path;
                path.local.addr = reinterpret_cast<ngtcp2_sockaddr*>(&pkt_locals[0]);
                path.local.addrlen = local_addrlen;
                path.remote.addr = reinterpret_cast<ngtcp2_sockaddr*>(&addrs[i]);
                path.remote.addrlen = msgs[i].msg_hdr.msg_namelen;
                batch[0].data = bufs[i];
                batch[0].len = pkt_len;
                ++batch_count;
            }
        }

        // Flush remaining batch
        if (batch_target && batch_count > 0) {
            batch_target->readPacketBatch(batch, batch_count, xsink);
            if (*xsink) {
                logBatchError(xsink, batch_target, batch_count);
            }
        }

        // If we received fewer than the batch size, the buffer is drained
        if (nrecv < QUIC_MAX_RECV_BATCH) {
            break;
        }
    }
    return 0;
#else
    // Drain all available incoming packets one at a time
    // Thread-local to reduce stack pressure (see QuicCommon.h for rationale)
    static thread_local struct sockaddr_storage src_addr;
    static thread_local uint8_t cmsg_buf[QUIC_CMSG_BUF_SIZE];
    while (true) {
        socklen_t src_addrlen = sizeof(src_addr);
        size_t cmsg_len = sizeof(cmsg_buf);
        ssize_t nread = recvQuicPacket(fd, recv_buf, recv_buf_size, MSG_DONTWAIT,
                                        reinterpret_cast<struct sockaddr*>(&src_addr), &src_addrlen,
                                        cmsg_buf, &cmsg_len);
        if (nread < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;  // no more data available
            }
            if (errno == EMSGSIZE) {
                continue;  // truncated datagram discarded; continue draining
            }
            xsink->raiseErrnoException("QUIC-RECV-ERROR", errno, "recvmsg() failed");
            return -1;
        }
        if (nread == 0) {
            return 0;
        }

        // Route to the correct session via dispatcher; drop packets
        // with unrecognized DCIDs rather than misrouting them.  Pin the session
        // through the owning map — a miss means the session was removed and the
        // packet must be dropped (see function docs)
        int64_t sid = dispatcher.dispatch(recv_buf, static_cast<size_t>(nread));
        if (sid < 0) {
            continue;
        }
        std::shared_ptr<QuicSession> target_sp = spriv->getQuicSession(sid);
        if (!target_sp) {
            continue;
        }
        QuicSession* target = target_sp.get();

        // Per-packet local address from pktinfo
        struct sockaddr_storage pkt_local;
        memcpy(&pkt_local, &local_addr, local_addrlen);
        extractPktinfoAddr(cmsg_buf, cmsg_len, local_addr.ss_family, &pkt_local);

        // Build ngtcp2 path with local and remote addresses
        ngtcp2_path path;
        path.local.addr = reinterpret_cast<ngtcp2_sockaddr*>(&pkt_local);
        path.local.addrlen = local_addrlen;
        path.remote.addr = reinterpret_cast<ngtcp2_sockaddr*>(&src_addr);
        path.remote.addrlen = src_addrlen;

        int rv = target->readPacket(recv_buf, static_cast<size_t>(nread), path, xsink);
        if (*xsink) {
            // Log exception details and continue; a single session error should not
            // stop the server from receiving packets for other sessions
            QoreStringValueHelper err_str(xsink->getExceptionErr());
            QoreStringValueHelper desc_str(xsink->getExceptionDesc());
            fprintf(stderr, "QUIC WARNING: readPacket error (session %lld%s): %s: %s\n",
                (long long)target->getSessionId(),
                target->isClosed() ? ", now closed" : "",
                *err_str ? err_str->c_str() : "unknown",
                *desc_str ? desc_str->c_str() : "unknown");
            xsink->clear();
        }
    }
#endif
}

// NOTE: sock->priv is accessible because these classes are friends of QoreSocketObject

// ============================================================
// SocketQuicClientPollOperation
// ============================================================

SocketQuicClientPollOperation::SocketQuicClientPollOperation(
    ExceptionSink* xsink, QoreSocketObject* sock,
    const char* host, uint16_t port, int family,
    int64_t handshake_timeout_ns,
    int64_t not_before_ns_abs)
    : SocketPollSocketOperationBase(sock), host(host), service(std::to_string(port)),
      handshake_timeout_ns_(handshake_timeout_ns), port_(port), family_(q_get_af(family)),
      not_before_ns_(not_before_ns_abs) {
    AutoLocker al(sock->priv->m);

    // Validate socket
    if (sock->priv->checkOpen(xsink)) {
        return;
    }
    if (sock->priv->socket->priv->stype != SOCK_DGRAM) {
        xsink->raiseException("QUIC-ERROR", "QUIC requires a UDP (SOCK_DGRAM) socket; this socket has type %d",
            sock->priv->socket->priv->stype);
        return;
    }
}

SocketQuicClientPollOperation::~SocketQuicClientPollOperation() = default;

QoreHashNode* SocketQuicClientPollOperation::getResolverPollInfo(ExceptionSink* xsink) const {
    std::vector<std::pair<int, int>> extra_fds;
    resolver->getExtraFds(extra_fds);
    QoreHashNode* rv = getSocketPollInfoHash(xsink, 0, extra_fds);
    if (*xsink) {
        return nullptr;
    }
    int poll_timeout_ms = resolver->getPollTimeoutMs();
    rv->setKeyValue("poll_timeout_ms", poll_timeout_ms >= 0 ? poll_timeout_ms : 1, xsink);
    return rv;
}

QoreHashNode* SocketQuicClientPollOperation::continueResolve(ExceptionSink* xsink) {
    if (!resolver) {
        resolver = std::make_unique<QoreCaresAddrInfoResolver>(host, service, family_, SOCK_DGRAM, 0);
    }

    int rc = resolver->continuePoll(xsink);
    if (*xsink || rc < 0) {
        return nullptr;
    }
    if (rc) {
        return getResolverPollInfo(xsink);
    }

    if (initializeResolved(xsink)) {
        return nullptr;
    }
    return nullptr;
}

int SocketQuicClientPollOperation::initializeResolved(ExceptionSink* xsink) {
    const std::vector<SocketResolvedAddrInfo>& addrs = resolver->getAddresses();
    if (addrs.empty()) {
        xsink->raiseException("QUIC-ERROR", "could not resolve remote address '%s:%s'",
            host.c_str(), service.c_str());
        return -1;
    }

    const SocketResolvedAddrInfo& ai = addrs.front();
    memcpy(&remote_addr_, &ai.addr, ai.addrlen);
    remote_addrlen_ = ai.addrlen;
    resolver.reset();

    AutoLocker al(sock->priv->m);
    if (sock->priv->checkOpen(xsink)) {
        return -1;
    }
    if (sock->priv->socket->priv->stype != SOCK_DGRAM) {
        xsink->raiseException("QUIC-ERROR", "QUIC requires a UDP (SOCK_DGRAM) socket; this socket has type %d",
            sock->priv->socket->priv->stype);
        return -1;
    }

    // Connect UDP socket to remote so getsockname() returns the specific
    // local interface address (not wildcard). Required for correct ngtcp2
    // path validation. Connected sockets also filter incoming packets to
    // only those from the peer, which is desirable for QUIC clients.
    int fd = sock->priv->socket->getSocket();
    if (::connect(fd, reinterpret_cast<const struct sockaddr*>(&remote_addr_), remote_addrlen_) < 0) {
        xsink->raiseErrnoException("QUIC-ERROR", errno, "UDP connect() failed");
        return -1;
    }

    // Enlarge UDP receive buffer to prevent packet drops under burst traffic.
    // QUIC sends many small datagrams; the kernel default (~208KB on Linux) is
    // easily exhausted when the server sends large responses.  1MB matches
    // common QUIC implementation defaults (e.g., Google, Cloudflare).
    // SO_RCVBUFFORCE bypasses net.core.rmem_max (requires CAP_NET_ADMIN);
    // fall back to SO_RCVBUF which is capped by rmem_max.
    {
        int rcvbuf = 1024 * 1024;
#ifdef SO_RCVBUFFORCE
        if (setsockopt(fd, SOL_SOCKET, SO_RCVBUFFORCE, &rcvbuf, sizeof(rcvbuf)) < 0)
#endif
        {
            setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
        }
    }

    // Cache local address after connect — now contains the specific local
    // interface address selected by the kernel (not wildcard)
    local_addrlen_ = sizeof(local_addr_);
    if (getsockname(fd, reinterpret_cast<struct sockaddr*>(&local_addr_), &local_addrlen_) < 0) {
        xsink->raiseErrnoException("QUIC-ERROR", errno, "getsockname() failed on QUIC client socket");
        return -1;
    }
    if (enableQuicPktinfo(fd, local_addr_.ss_family) < 0) {
        printd(0, "SocketQuicClientPollOperation: enableQuicPktinfo() failed: errno=%d (%s)\n",
            errno, strerror(errno));
    }

    // Set non-blocking guard flag on QoreSocketObject
    if (sock->priv->setNonBlock(xsink)) {
        return -1;
    }
    set_non_block = true;

    // Set OS-level non-blocking mode on the fd — required because QUIC uses
    // raw recvfrom()/sendto() on the UDP socket, bypassing Qore's timeout-based
    // poll wrappers.  Without this, recvfrom() blocks indefinitely.
    if (sock->priv->socket->priv->set_non_blocking(true, xsink)) {
        sock->priv->clearNonBlock();
        set_non_block = false;
        return -1;
    }

    // Create QUIC session with actual socket addresses; pass through the
    // socket's ssl_verify_mode so SSL_CTX_set_verify() enforces verification;
    // enable 0-RTT for reconnections with cached session tickets.
    // Pass client cert/pk for mTLS if configured on the socket.
    quic_session = QuicSession::createClient(sock->priv->socket->priv, xsink, host.c_str(), port_,
        reinterpret_cast<const struct sockaddr*>(&local_addr_), local_addrlen_,
        reinterpret_cast<const struct sockaddr*>(&remote_addr_), remote_addrlen_,
        sock->priv->socket->priv->ssl_verify_mode,
        /*enable_0rtt=*/true,
        sock->priv->cert, sock->priv->pk);
    if (*xsink || !quic_session) {
        sock->priv->clearNonBlock();
        set_non_block = false;
        return -1;
    }

    // Store the session on qore_socket_private for access via Socket QPP methods
    sock->priv->socket->priv->addQuicSession(quic_session);

    // Record the absolute handshake deadline (if a connect_timeout was supplied).
    // Without this, a stalled QUIC handshake over UDP has no transport-level
    // deadline (unlike TCP where the OS eventually returns ETIMEDOUT) — only
    // the outer request_timeout would bound it.
    if (handshake_timeout_ns_ > 0) {
        handshake_deadline_ns_ = QuicSession::timestamp() + handshake_timeout_ns_;
    }

    qcs_state = QCS::HANDSHAKE_SEND;
    return 0;
}

const char* SocketQuicClientPollOperation::getStateImpl() const {
    switch (qcs_state) {
        case QCS::NONE: return resolver ? "resolving" : "initializing";
        case QCS::HANDSHAKE_SEND: return "handshake-send";
        case QCS::HANDSHAKE_RECV: return "handshake-recv";
        case QCS::SETUP_HTTP3: return "setup-http3";
        case QCS::READING: return "reading";
        case QCS::RESPONSE_READY: return "response-ready";
        case QCS::CLOSED: return "closed";
        default: return "unknown";
    }
}

QoreValue SocketQuicClientPollOperation::getOutput() const {
    if (!cached_stream) {
        return QoreValue();
    }

    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), nullptr);

    h->setKeyValue("session_id", quic_session ? quic_session->getSessionId() : (int64_t)0, nullptr);
    h->setKeyValue("status_code", cached_stream->status_code, nullptr);
    h->setKeyValue("method", new QoreStringNode(cached_stream->method), nullptr);
    h->setKeyValue("path", new QoreStringNode(cached_stream->path), nullptr);
    h->setKeyValue("stream_id", cached_stream->stream_id, nullptr);

    if (!cached_stream->error_message.empty()) {
        // Use a more specific error code for peer-reset streams; keep
        // QUIC-BODY-TOO-LARGE for the request-body-exceeded case (legacy).
        const char* err_code = "QUIC-STREAM-ERROR";
        if (cached_stream->error_message.find("exceeded maximum size")
                != std::string::npos) {
            err_code = "QUIC-BODY-TOO-LARGE";
        } else if (cached_stream->error_message.find("peer reset")
                != std::string::npos) {
            err_code = "QUIC-STREAM-RESET";
        }
        h->setKeyValue("err", new QoreStringNode(err_code), nullptr);
        h->setKeyValue("desc", new QoreStringNode(cached_stream->error_message), nullptr);
    }

    // Headers
    ReferenceHolder<QoreHashNode> headers(new QoreHashNode(autoTypeInfo), nullptr);
    for (const auto& hdr : cached_stream->headers) {
        if (hdr.second.size() == 1) {
            headers->setKeyValue(hdr.first.c_str(), new QoreStringNode(hdr.second[0]), nullptr);
        } else {
            ReferenceHolder<QoreListNode> values(new QoreListNode(autoTypeInfo), nullptr);
            for (const auto& v : hdr.second) {
                values->push(new QoreStringNode(v), nullptr);
            }
            headers->setKeyValue(hdr.first.c_str(), values.release(), nullptr);
        }
    }
    h->setKeyValue("headers", headers.release(), nullptr);

    // Body
    if (!cached_stream->body.empty()) {
        SimpleRefHolder<BinaryNode> body(new BinaryNode());
        body->append(cached_stream->body.data(), cached_stream->body.size());
        h->setKeyValue("body", body.release(), nullptr);
    }

    // Flag whether response is complete.  Currently always true for client
    // poll operations because takeCompletedStream() only returns streams
    // after markStreamComplete() sets body_complete=true.  Kept as a
    // dynamic value for forward compatibility with incremental delivery.
    h->setKeyValue("end_stream", cached_stream->body_complete, nullptr);

    // Consume the cached stream so subsequent calls return NOTHING
    cached_stream.reset();

    return h.release();
}

int SocketQuicClientPollOperation::sendPendingPackets(
    ngtcp2_tstamp& next_expiry, ExceptionSink* xsink) {
    // Happy-eyeballs Connection Attempt Delay (RFC 8305 §8).  When this
    // operation is a secondary address-family attempt staggered behind the
    // preferred family, suppress all datagram emission until the deadline
    // elapses.  processTimerAndWrite() is skipped entirely so ngtcp2 does
    // not even generate Initial packets — the session stays idle.  Return
    // SOCK_POLLOUT with next_expiry = not_before_ns_ so the caller yields
    // back to the controller with a timeout equal to the remaining stagger,
    // rather than falling through to HANDSHAKE_RECV.
    if (not_before_ns_) {
        ngtcp2_tstamp now = QuicSession::timestamp();
        if (now < not_before_ns_) {
            next_expiry = not_before_ns_;
            ASYNC_IO_TRACE("SocketQuicClientPollOperation::sendPendingPackets "
                           "HE_STAGGER wait_ns=%lld\n",
                           (long long)(not_before_ns_ - now));
            return SOCK_POLLOUT;
        }
        // Deadline reached; disable the gate so subsequent calls proceed
        // normally.  ngtcp2's own PTO timer now governs retransmission.
        not_before_ns_ = 0;
    }

    // Coalesced timer check + packet generation under a single lock
    auto result = quic_session->processTimerAndWrite(pkt_batch_, xsink);
    if (result.error) {
        pkt_batch_.clear();
        return -1;
    }
    next_expiry = result.next_expiry;

    if (pkt_batch_.empty()) {
        ASYNC_IO_TRACE("SocketQuicClientPollOperation::sendPendingPackets EMPTY (no packets to send)\n");
        return 0;
    }

    int fd = sock->priv->socket->getSocket();

    int sent = sendQuicPacketsBatch(fd, pkt_batch_, nullptr, 0);
    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            ASYNC_IO_TRACE("SocketQuicClientPollOperation::sendPendingPackets EAGAIN fd=%d batch=%d\n",
                fd, pkt_batch_.size());
            return SOCK_POLLOUT;
        }
        pkt_batch_.clear();
        xsink->raiseErrnoException("QUIC-SEND-ERROR", errno, "sendto/sendmmsg() failed");
        return -1;
    }
    ASYNC_IO_TRACE("SocketQuicClientPollOperation::sendPendingPackets SENT fd=%d sent=%d/%d\n",
        fd, sent, pkt_batch_.size());
    if (sent > 0 && sent < pkt_batch_.size()) {
        printd(1, "SocketQuicClientPollOperation::sendPendingPackets(): partial QUIC send: %d/%d packets\n",
            sent, pkt_batch_.size());
        // Retain unsent packets for the next send cycle instead of dropping them;
        // ngtcp2 would retransmit via PTO timers, but sending them promptly avoids
        // the unnecessary delay and bandwidth overhead of retransmission
        pkt_batch_.removeFront(sent);
        return SOCK_POLLOUT;
    }

    pkt_batch_.clear();
    return 0;
}

int SocketQuicClientPollOperation::recvAndProcessPacket(ExceptionSink* xsink) {
    // Thread-local to reduce stack pressure (see QuicCommon.h for rationale)
    static thread_local struct sockaddr_storage src_addr;
    static thread_local uint8_t cmsg_buf[QUIC_CMSG_BUF_SIZE];
    socklen_t src_addrlen = sizeof(src_addr);

    int fd = sock->priv->socket->getSocket();
    size_t cmsg_len = sizeof(cmsg_buf);
    ssize_t nread = recvQuicPacket(fd, recv_buf_, sizeof(recv_buf_), 0,
                                    reinterpret_cast<struct sockaddr*>(&src_addr), &src_addrlen,
                                    cmsg_buf, &cmsg_len);
    if (nread < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return SOCK_POLLIN;  // need to wait for read
        }
        if (errno == EMSGSIZE) {
            return 0;  // truncated datagram discarded; continue draining
        }
        if (errno == ECONNREFUSED) {
            // ICMP "port unreachable" on a connected UDP socket.  Linux
            // rate-limits ICMP unreachable messages (~1/sec/peer), so a closed
            // UDP port produces exactly one ECONNREFUSED on recv even though
            // every retransmit triggers a fresh ICMP — counting consecutive
            // refusals therefore can't distinguish a permanently closed port
            // from a momentary bind race.  Instead, on the first refusal
            // arm a short grace deadline; if the handshake hasn't progressed
            // by then, continuePoll() fails the op with QUIC-CONNECT-REFUSED.
            // A successful recv (handshake response from a peer that recovered)
            // disarms the deadline.  Without this, a closed peer port (e.g.,
            // Alt-Svc h3=":1" with nothing listening) burns the entire
            // connect_timeout retransmitting Initials that will never be
            // answered.
            if (econnrefused_deadline_ns_ == 0
                && (qcs_state == QCS::HANDSHAKE_SEND
                    || qcs_state == QCS::HANDSHAKE_RECV)) {
                econnrefused_deadline_ns_ =
                    QuicSession::timestamp() + QUIC_ECONNREFUSED_GRACE_NS;
            }
            return SOCK_POLLIN;  // need to wait for read
        }
        xsink->raiseErrnoException("QUIC-RECV-ERROR", errno, "recvmsg() failed");
        return -1;
    }

    if (nread == 0) {
        return 0;
    }

    // Successful recv: peer is responsive — disarm the unreachable deadline.
    econnrefused_deadline_ns_ = 0;

    // Use the cached getsockname() address directly — the client socket is
    // connect()ed, so the local address is always local_addr_.  Do NOT call
    // extractPktinfoAddr(): on macOS, IP_RECVDSTADDR on connected UDP sockets
    // returns 0.0.0.0 instead of the actual bound address, causing ngtcp2 path
    // validation to fail with "ignore packet from unknown path".
    ngtcp2_path path;
    path.local.addr = reinterpret_cast<ngtcp2_sockaddr*>(&local_addr_);
    path.local.addrlen = local_addrlen_;
    path.remote.addr = reinterpret_cast<ngtcp2_sockaddr*>(&src_addr);
    path.remote.addrlen = src_addrlen;

    printd(5, "SocketQuicClientPollOperation::recvAndProcessPacket() received %d bytes\n", (int)nread);
    int rv = quic_session->readPacket(recv_buf_, static_cast<size_t>(nread), path, xsink);
    if (*xsink) {
        return -1;
    }

    return 0;
}

QoreHashNode* SocketQuicClientPollOperation::trySetupEarlyHttp3(ExceptionSink* xsink) {
    if (!quic_session->isEarlyDataReady() || quic_session->isHttp3Ready()) {
        return nullptr;  // no-op: not attempting 0-RTT or already set up
    }
    quic_session->setupHttp3(xsink);
    if (*xsink) {
        return nullptr;
    }
    // Flush HTTP/3 setup frames as 0-RTT packets
    ngtcp2_tstamp h3_expiry;
    int srv = sendPendingPackets(h3_expiry, xsink);
    if (*xsink) {
        return nullptr;
    }
    if (srv == SOCK_POLLOUT) {
        // Partial send — caller should yield for write readiness
        QoreHashNode* poll_info = getSocketPollInfoHash(xsink, SOCK_POLLOUT);
        if (poll_info) {
            setPollTimeoutFromExpiry(poll_info, h3_expiry, xsink);
        }
        return poll_info;
    }
    return nullptr;
}

QoreHashNode* SocketQuicClientPollOperation::flushAndReturnPollInfo(ExceptionSink* xsink,
        bool do_flush) {
    // Check if the QUIC session is dead (CONNECTION_CLOSE received, idle timeout,
    // or draining period).  Without this, continuePoll returns goal=0 (POLLIN)
    // forever on a dead connection, causing a livelock in the async I/O controller.
    if (quic_session->isClosed()) {
        qcs_state = QCS::CLOSED;
        if (set_non_block) {
            sock->priv->clearNonBlock();
            set_non_block = false;
        }
        return nullptr;
    }
    ngtcp2_tstamp expiry = 0;
    int srv = 0;
    if (do_flush) {
        srv = sendPendingPackets(expiry, xsink);
        if (*xsink) {
            return nullptr;
        }
        // Curl pattern (curl_ngtcp2.c:928): after handle_expiry + egress,
        // re-check expiry.  If still in the past, do one more handle+flush
        // cycle to drain any timers that re-armed during the write.
        if (expiry != UINT64_MAX && expiry <= QuicSession::timestamp()) {
            srv = sendPendingPackets(expiry, xsink);
            if (*xsink) {
                return nullptr;
            }
        }
        // Check for idle close after timer processing
        if (quic_session->isClosed()) {
            qcs_state = QCS::CLOSED;
            if (set_non_block) {
                sock->priv->clearNonBlock();
                set_non_block = false;
            }
            return nullptr;
        }
    } else {
        // When not flushing, use the QUIC connection's next expiry for the poll
        // timeout so retransmission timers still fire
        expiry = quic_session->getExpiry();
    }
    // Request write-readiness (POLLOUT) ONLY when sendPendingPackets reported a
    // genuine socket-level partial send (SOCK_POLLOUT: the kernel UDP send buffer
    // filled).  Do NOT request POLLOUT merely because hasPendingWrite() is true:
    // when sendPendingPackets already drained everything it could and data still
    // remains, that data is QUIC flow-control / congestion-window blocked, not
    // socket-blocked.  A UDP socket is almost always writable, so requesting
    // POLLOUT for flow-control-blocked data busy-spins the I/O thread on an
    // always-ready POLLOUT (issue #5351).  Such data is unblocked by an INCOMING
    // ACK / MAX_STREAM_DATA, so we wait for POLLIN (plus the retransmission timer
    // for PTO probes) — matching the pending_write_ design comment in
    // QuicSession::writePacketsLocked ("the next read cycle ... triggers another
    // write attempt").
    int events = SOCK_POLLIN;
    if (srv == SOCK_POLLOUT) {
        events |= SOCK_POLLOUT;
    }
    QoreHashNode* poll_info = getSocketPollInfoHash(xsink, events);
    if (poll_info) {
        // nothing_to_send: the flush produced no packet send (srv==0) yet data may
        // still be pending (flow-control-blocked) — avoid sub-ms tight-polling.
        setPollTimeoutFromExpiry(poll_info, expiry, xsink, srv == 0);
    }
    return poll_info;
}

QoreHashNode* SocketQuicClientPollOperation::continuePoll(ExceptionSink* xsink) {
    if (qcs_state == QCS::NONE) {
        QoreHashNode* poll_info = continueResolve(xsink);
        if (*xsink || poll_info || qcs_state != QCS::HANDSHAKE_SEND) {
            return poll_info;
        }
    }

    AutoLocker al(sock->priv->m);

    if (!sock->priv->socket->priv->isOpen()) {
        xsink->raiseException("QUIC-POLL-ERROR", "socket closed during poll operation");
        return nullptr;
    }

    // Guard against continuePoll() after abort() has reset quic_session
    if (!quic_session) {
        xsink->raiseException("QUIC-POLL-ERROR", "QUIC client session has been aborted");
        return nullptr;
    }

    // Enforce the handshake deadline if one was configured. QUIC runs over UDP,
    // so a stalled handshake gets no OS-level feedback (unlike TCP connect which
    // eventually returns ETIMEDOUT). Without this check, the outer request_timeout
    // is the only bound, producing an opaque HTTPCLIENT-TIMEOUT rather than a
    // diagnosable connect-phase error at the configured connect_timeout.
    if (handshake_deadline_ns_ > 0
        && (qcs_state == QCS::HANDSHAKE_SEND || qcs_state == QCS::HANDSHAKE_RECV)
        && QuicSession::timestamp() >= handshake_deadline_ns_) {
        qcs_state = QCS::CLOSED;
        xsink->raiseException("QUIC-HANDSHAKE-TIMEOUT",
            "QUIC handshake did not complete within the configured connect_timeout");
        return nullptr;
    }

    // Fast-fail a handshake whose peer has signalled ICMP port-unreachable
    // and not recovered within the grace window — see
    // @ref econnrefused_deadline_ns_ for the rate-limit rationale.
    if (econnrefused_deadline_ns_ > 0
        && (qcs_state == QCS::HANDSHAKE_SEND || qcs_state == QCS::HANDSHAKE_RECV)
        && QuicSession::timestamp() >= econnrefused_deadline_ns_) {
        qcs_state = QCS::CLOSED;
        xsink->raiseException("QUIC-CONNECT-REFUSED",
            "QUIC handshake target %s:%u refused datagrams (ICMP port "
            "unreachable) and did not recover within %lld ms; peer UDP port "
            "appears closed",
            host.c_str(), static_cast<unsigned>(port_),
            (long long)(QUIC_ECONNREFUSED_GRACE_NS / 1000000LL));
        return nullptr;
    }

    while (true) {
        switch (qcs_state) {
            case QCS::HANDSHAKE_SEND: {
                // Generate and send handshake packets (coalesced: timer + write)
                ngtcp2_tstamp next_expiry;
                int rv = sendPendingPackets(next_expiry, xsink);
                if (*xsink) {
                    return nullptr;
                }
                if (rv == SOCK_POLLOUT) {
                    QoreHashNode* poll_info = getSocketPollInfoHash(xsink, SOCK_POLLOUT);
                    if (poll_info) {
                        setPollTimeoutFromExpiry(poll_info, next_expiry, xsink);
                        clampPollTimeoutToDeadline(poll_info, handshake_deadline_ns_, xsink);
                        clampPollTimeoutToDeadline(poll_info, econnrefused_deadline_ns_, xsink);
                    }
                    return poll_info;
                }

                // 0-RTT: set up HTTP/3 early when 0-RTT TX key is installed
                {
                    QoreHashNode* poll_info = trySetupEarlyHttp3(xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (poll_info) {
                        return poll_info;  // SOCK_POLLOUT — yield for write readiness
                    }
                }

                qcs_state = QCS::HANDSHAKE_RECV;
                [[fallthrough]];
            }

            case QCS::HANDSHAKE_RECV: {
                // Drain all available handshake datagrams
                while (true) {
                    int rv = recvAndProcessPacket(xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (rv == SOCK_POLLIN) {
                        break;  // EAGAIN — no more data
                    }
                }

                // Check if connection closed during handshake (e.g., TLS
                // rejection via CONNECTION_CLOSE).  ngtcp2_conn_read_pkt()
                // returns 0 when processing a CONNECTION_CLOSE and silently
                // enters DRAINING state — without this check the handshake
                // loop would poll forever.
                if (quic_session->isClosed()) {
                    ngtcp2_ccerr ccerr = quic_session->getCloseError();
                    xsink->raiseException("QUIC-HANDSHAKE-ERROR",
                        "QUIC handshake failed: connection closed by peer "
                        "(type=%d, error_code=0x%llx)",
                        (int)ccerr.type, (long long)ccerr.error_code);
                    return nullptr;
                }

                // 0-RTT: set up HTTP/3 early when 0-RTT TX key is installed
                // (may be detected after receiving server response to Initial)
                {
                    QoreHashNode* poll_info = trySetupEarlyHttp3(xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (poll_info) {
                        qcs_state = QCS::HANDSHAKE_SEND;
                        return poll_info;  // SOCK_POLLOUT — yield for write readiness
                    }
                }

                // Check if handshake completed
                if (quic_session->isHandshakeComplete()) {
                    qcs_state = QCS::SETUP_HTTP3;
                } else {
                    // Send any pending handshake data (coalesced: timer + write),
                    // then yield to poll for the next round of datagrams
                    ngtcp2_tstamp next_expiry;
                    {
                        int rv = sendPendingPackets(next_expiry, xsink);
                        if (*xsink) {
                            return nullptr;
                        }
                    }
                    qcs_state = QCS::HANDSHAKE_RECV;
                    // Compute QUIC-aware poll timeout hint so retransmission
                    // timers fire even when no packets arrive (lossy networks)
                    QoreHashNode* poll_info = getSocketPollInfoHash(xsink, SOCK_POLLIN);
                    if (poll_info) {
                        setPollTimeoutFromExpiry(poll_info, next_expiry, xsink);
                        clampPollTimeoutToDeadline(poll_info, handshake_deadline_ns_, xsink);
                        clampPollTimeoutToDeadline(poll_info, econnrefused_deadline_ns_, xsink);
                    }
                    return poll_info;
                }
                [[fallthrough]];
            }

            case QCS::SETUP_HTTP3: {
                // Handle 0-RTT rejection: if HTTP/3 was set up during 0-RTT but
                // early data was rejected, re-initialize with new streams
                if (quic_session->isEarlyDataRejected() && quic_session->isHttp3Ready()) {
                    quic_session->resetHttp3(xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                }

                // Set up HTTP/3 layer (no-op if already set up during 0-RTT)
                if (!quic_session->isHttp3Ready()) {
                    quic_session->setupHttp3(xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                }

                // Send any pending frames (HTTP/3 control + QPACK streams)
                // Coalesced: timer + write
                ngtcp2_tstamp setup_expiry;
                {
                    int rv = sendPendingPackets(setup_expiry, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (rv == SOCK_POLLOUT) {
                        QoreHashNode* poll_info = getSocketPollInfoHash(xsink, SOCK_POLLOUT);
                        if (poll_info) {
                            setPollTimeoutFromExpiry(poll_info, setup_expiry, xsink);
                        }
                        return poll_info;
                    }
                }

                qcs_state = QCS::READING;
                {
                    QoreHashNode* poll_info = getSocketPollInfoHash(xsink, SOCK_POLLIN);
                    if (poll_info) {
                        setPollTimeoutFromExpiry(poll_info, setup_expiry, xsink);
                    }
                    return poll_info;
                }
            }

            case QCS::READING: {
                // Check for completed streams
                if (quic_session->hasCompletedStreams()) {
                    cached_stream = quic_session->takeCompletedStream();
                    qcs_state = QCS::RESPONSE_READY;
                    // Flush pending writes (ACKs) before signaling goal reached
                    {
                        QoreHashNode* flush_info = flushAndReturnPollInfo(xsink);
                        if (flush_info) {
                            flush_info->deref(xsink);
                        }
                    }
                    return nullptr;  // goal reached
                }

                // Drain available datagrams from the socket buffer, up to
                // the per-call budget.  Each recvfrom() returns exactly one
                // UDP datagram; the budget ensures fair scheduling when
                // multiple QUIC connections share the same I/O thread.
                int packets_processed = 0;
                bool budget_exhausted = false;
                while (packets_processed < QUIC_CLIENT_RECV_BUDGET) {
                    int rv = recvAndProcessPacket(xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (rv == SOCK_POLLIN) {
                        break;  // EAGAIN — no more data
                    }
                    ++packets_processed;
                    // Check for completed streams after each datagram
                    if (quic_session->hasCompletedStreams()) {
                        cached_stream = quic_session->takeCompletedStream();
                        qcs_state = QCS::RESPONSE_READY;
                        // Flush pending writes (ACKs) before signaling goal reached
                        {
                            QoreHashNode* flush_info = flushAndReturnPollInfo(xsink);
                            if (flush_info) {
                                flush_info->deref(xsink);
                            }
                        }
                        return nullptr;  // goal reached
                    }
                }
                if (packets_processed >= QUIC_CLIENT_RECV_BUDGET) {
                    budget_exhausted = true;
                }

                // Check if connection closed
                if (quic_session->isClosed()) {
                    qcs_state = QCS::CLOSED;
                    sock->priv->clearNonBlock();
                    set_non_block = false;
                    return nullptr;
                }

                // Send any pending data (ACKs, HTTP/3 request frames, etc.)
                // Coalesced: timer + write
                {
                    ngtcp2_tstamp next_expiry;
                    int srv = sendPendingPackets(next_expiry, xsink);
                    if (*xsink) {
                        return nullptr;
                    }

                    // sendPendingPackets runs the timer-expiry handler, which
                    // may have transitioned the session to the idle-closed
                    // state (RFC 9000 Section 10.1 silent close).  Check
                    // immediately so the UDP fd is released in the same
                    // continuePoll cycle instead of waiting another round.
                    if (quic_session->isClosed()) {
                        qcs_state = QCS::CLOSED;
                        sock->priv->clearNonBlock();
                        set_non_block = false;
                        return nullptr;
                    }

                    // Check again for completed streams
                    if (quic_session->hasCompletedStreams()) {
                        cached_stream = quic_session->takeCompletedStream();
                        qcs_state = QCS::RESPONSE_READY;
                        // Flush pending writes (ACKs) before signaling goal reached;
                        // sendPendingPackets may have triggered internal processing
                        // that generated new ACK obligations
                        {
                            QoreHashNode* flush_info = flushAndReturnPollInfo(xsink);
                            if (flush_info) {
                                flush_info->deref(xsink);
                            }
                        }
                        return nullptr;  // goal reached
                    }

                    // After sending (e.g. HTTP/3 request frames), try a
                    // non-blocking recv before falling back to poll().  On
                    // low-latency paths (localhost, same-host), the server
                    // response may already be in the kernel socket buffer
                    // by the time sendPendingPackets() returns — this
                    // eliminates one full poll() syscall round-trip.
                    // Use remaining budget from the first recv loop.
                    while (packets_processed < QUIC_CLIENT_RECV_BUDGET) {
                        int rrv = recvAndProcessPacket(xsink);
                        if (*xsink) {
                            return nullptr;
                        }
                        if (rrv == SOCK_POLLIN) {
                            break;  // EAGAIN — no more data
                        }
                        ++packets_processed;
                        if (quic_session->hasCompletedStreams()) {
                            cached_stream = quic_session->takeCompletedStream();
                            qcs_state = QCS::RESPONSE_READY;
                            // Flush pending writes (ACKs) before signaling goal
                            // reached; recvAndProcessPacket generated new ACK
                            // obligations after sendPendingPackets
                            {
                                QoreHashNode* flush_info = flushAndReturnPollInfo(xsink);
                                if (flush_info) {
                                    flush_info->deref(xsink);
                                }
                            }
                            return nullptr;  // goal reached
                        }
                    }
                    if (packets_processed >= QUIC_CLIENT_RECV_BUDGET) {
                        budget_exhausted = true;
                    }

                    // Flush any pending frames generated by the fast-path recv
                    // (e.g. MAX_STREAM_DATA for flow control).  Without this,
                    // the server may be blocked waiting for a flow-control
                    // update that sits unsent until the next continuePoll().
                    {
                        ngtcp2_tstamp fast_expiry;
                        int fast_rv = sendPendingPackets(fast_expiry, xsink);
                        if (*xsink) {
                            return nullptr;
                        }
                        // Use the tighter expiry / write requirement
                        if (fast_rv == SOCK_POLLOUT) {
                            srv = fast_rv;
                        }
                        if (fast_expiry < next_expiry) {
                            next_expiry = fast_expiry;
                        }
                    }

                    // Register for POLLIN always; add POLLOUT ONLY for a genuine
                    // socket-level partial send (SOCK_POLLOUT).  Do NOT add POLLOUT
                    // for hasPendingWrite() — UDP sockets are almost always writable,
                    // so flow-control / congestion-window-blocked data (which
                    // sendPendingPackets could not drain) would busy-loop on an
                    // always-ready POLLOUT; it is unblocked by an incoming ACK /
                    // MAX_STREAM_DATA (POLLIN) plus the PTO timer (issue #5351; see
                    // the identical guard in continuePoll()'s established-path branch).
                    int events = SOCK_POLLIN;
                    if (srv == SOCK_POLLOUT) {
                        events |= SOCK_POLLOUT;
                    }

                    // Compute QUIC-aware poll timeout hint so retransmission
                    // timers fire even when no packets arrive
                    QoreHashNode* poll_info = getSocketPollInfoHash(xsink, events);
                    if (poll_info) {
                        if (budget_exhausted) {
                            // More packets may remain in the socket buffer;
                            // request immediate re-poll for fair scheduling
                            poll_info->setKeyValue("poll_timeout_ms", 0, xsink);
                        } else {
                            setPollTimeoutFromExpiry(poll_info, next_expiry, xsink, srv == 0);
                        }
                    }
                    return poll_info;
                }
            }

            case QCS::RESPONSE_READY: {
                // Already reached goal; if called again, go back to reading.
                // Non-blocking mode is maintained throughout (no toggle per
                // response) — the QUIC socket always uses raw recvfrom/sendto
                if (quic_session->isClosed()) {
                    qcs_state = QCS::CLOSED;
                    sock->priv->clearNonBlock();
                    set_non_block = false;
                    return nullptr;
                }
                if (quic_session->hasCompletedStreams()) {
                    cached_stream = quic_session->takeCompletedStream();
                    return flushAndReturnPollInfo(xsink);
                }
                cached_stream.reset();
                qcs_state = QCS::READING;
                continue;
            }

            case QCS::CLOSED:
                if (set_non_block) {
                    sock->priv->clearNonBlock();
                    set_non_block = false;
                }
                return nullptr;

            default:
                xsink->raiseException("QUIC-POLL-ERROR", "invalid poll state: %d", static_cast<int>(qcs_state));
                return nullptr;
        }
    }
}

int SocketQuicClientPollOperation::migrateConnection(ExceptionSink* xsink) {
    // Create a new connected UDP socket and swap the fd on the socket object.
    // The old fd is closed immediately — this removes it from epoll so the
    // controller's next continuePoll cycle re-adds the new fd cleanly.

    if (!quic_session) {
        xsink->raiseException("QUIC-MIGRATION-ERROR", "QUIC session not initialized");
        return -1;
    }

    int old_fd = sock->priv->socket->getSocket();
    if (old_fd < 0) {
        xsink->raiseException("QUIC-MIGRATION-ERROR", "socket is not open");
        return -1;
    }

    // RAII guard: auto-closes the fd on error paths; release() on success
    struct FdGuard {
        int fd;
        FdGuard(int fd) : fd(fd) {}
        ~FdGuard() { if (fd >= 0) { ::close(fd); } }
        int release() { int r = fd; fd = -1; return r; }
    };

    // Create a new UDP socket with the same address family
#ifdef SOCK_CLOEXEC
    int new_fd = ::socket(remote_addr_.ss_family, SOCK_DGRAM | SOCK_CLOEXEC, 0);
#else
    int new_fd = ::socket(remote_addr_.ss_family, SOCK_DGRAM, 0);
#endif
    if (new_fd < 0) {
        xsink->raiseErrnoException("QUIC-MIGRATION-ERROR", errno,
            "failed to create new UDP socket for migration");
        return -1;
    }
#ifndef SOCK_CLOEXEC
    fcntl(new_fd, F_SETFD, FD_CLOEXEC);
#endif
    FdGuard fd_guard(new_fd);

    // Connect the new socket to the same remote address
    if (::connect(new_fd, reinterpret_cast<const struct sockaddr*>(&remote_addr_),
                  remote_addrlen_) < 0) {
        xsink->raiseErrnoException("QUIC-MIGRATION-ERROR", errno,
            "UDP connect() failed for migration socket");
        return -1;
    }

    // Enlarge receive buffer (same as initial socket)
    {
        int rcvbuf = 1024 * 1024;
#ifdef SO_RCVBUFFORCE
        if (setsockopt(new_fd, SOL_SOCKET, SO_RCVBUFFORCE, &rcvbuf, sizeof(rcvbuf)) < 0)
#endif
        {
            setsockopt(new_fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
        }
    }

    // Get the new local address
    struct sockaddr_storage new_local;
    socklen_t new_local_len = sizeof(new_local);
    if (::getsockname(new_fd, reinterpret_cast<struct sockaddr*>(&new_local),
                      &new_local_len) < 0) {
        xsink->raiseErrnoException("QUIC-MIGRATION-ERROR", errno,
            "getsockname() failed on migration socket");
        return -1;
    }

    // Enable pktinfo (non-fatal for client sockets)
    if (enableQuicPktinfo(new_fd, new_local.ss_family) < 0) {
        printd(2, "SocketQuicClientPollOperation::migrateConnection(): enableQuicPktinfo() "
            "failed (non-fatal for client): errno=%d (%s)\n", errno, strerror(errno));
    }

    // Set non-blocking mode on the new socket
    int flags = fcntl(new_fd, F_GETFL, 0);
    if (flags < 0 || fcntl(new_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        xsink->raiseErrnoException("QUIC-MIGRATION-ERROR", errno,
            "failed to set non-blocking mode on migration socket");
        return -1;
    }

    // Initiate migration at the QUIC layer
    if (quic_session->initiateMigration(
            reinterpret_cast<const struct sockaddr*>(&new_local), new_local_len,
            reinterpret_cast<const struct sockaddr*>(&remote_addr_), remote_addrlen_,
            xsink) != 0) {
        return -1;
    }

    // Swap the fd under the socket lock to synchronize with any concurrent
    // I/O.  Since migrateConnection() is now called from the I/O thread
    // (via the migration_pending flag), there is no concurrent poll() on the
    // old fd — the epoll registration is cleanly replaced on the next cycle.
    {
        AutoLocker al(sock->priv->m);
        fd_guard.release();
        sock->priv->socket->priv->sock = new_fd;
        // Bump fd_generation so any sync I/O helper mid-wait (lock
        // released) returns QSE_NOT_OPEN on re-acquire instead of
        // operating on the pre-migration fd.
        ++sock->priv->socket->priv->fd_generation;
        memcpy(&local_addr_, &new_local, new_local_len);
        local_addrlen_ = new_local_len;
    }

    // Close old fd outside the lock — safe because the socket object now
    // points to new_fd
    ::close(old_fd);

    // Send PATH_CHALLENGE + pending request frames immediately so the server
    // learns the new source address before the next poll() cycle
    {
        ngtcp2_tstamp expiry;
        sendPendingPackets(expiry, xsink);
        if (*xsink) {
            xsink->handleExceptions();
        }
    }

    printd(2, "SocketQuicClientPollOperation::migrateConnection(): migrated fd %d -> %d "
        "(session %lld)\n", old_fd, new_fd,
        (long long)quic_session->getSessionId());

    return 0;
}

// ============================================================
// SocketQuicServerPollOperation
// ============================================================

SocketQuicServerPollOperation::SocketQuicServerPollOperation(
    ExceptionSink* xsink, QoreSocketObject* sock)
    : SocketPollSocketOperationBase(sock) {
    AutoLocker al(sock->priv->m);

    // Validate socket
    if (sock->priv->checkOpen(xsink)) {
        return;
    }
    if (sock->priv->socket->priv->stype != SOCK_DGRAM) {
        xsink->raiseException("QUIC-ERROR", "QUIC requires a UDP (SOCK_DGRAM) socket; this socket has type %d",
            sock->priv->socket->priv->stype);
        return;
    }

    // Set non-blocking guard flag on QoreSocketObject
    if (sock->priv->setNonBlock(xsink)) {
        return;
    }
    set_non_block = true;

    // Set OS-level non-blocking mode on the fd — required because QUIC uses
    // raw recvfrom()/sendto() on the UDP socket
    if (sock->priv->socket->priv->set_non_blocking(true, xsink)) {
        sock->priv->clearNonBlock();
        set_non_block = false;
        return;
    }

    // Cache local address (family + port); per-packet destination IP is
    // extracted from pktinfo control messages in recvAndProcessPacket()
    int fd = sock->priv->socket->getSocket();

    // Enlarge UDP receive buffer to prevent packet drops under burst traffic.
    // QUIC servers may receive bursts of client traffic (handshake retries,
    // multiplexed requests); 1MB matches common QUIC implementation defaults.
    {
        int rcvbuf = 1024 * 1024;
#ifdef SO_RCVBUFFORCE
        if (setsockopt(fd, SOL_SOCKET, SO_RCVBUFFORCE, &rcvbuf, sizeof(rcvbuf)) < 0)
#endif
        {
            setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
        }
    }

    local_addrlen_ = sizeof(local_addr_);
    if (getsockname(fd, reinterpret_cast<struct sockaddr*>(&local_addr_), &local_addrlen_) < 0) {
        xsink->raiseErrnoException("QUIC-ERROR", errno, "getsockname() failed on QUIC server socket");
        return;
    }
    if (enableQuicPktinfo(fd, local_addr_.ss_family) < 0) {
        printd(0, "SocketQuicServerPollOperation: enableQuicPktinfo() failed: errno=%d (%s)\n",
            errno, strerror(errno));
    }

    // Copy sessions from qore_socket_private (from previous poll ops).
    // This is a snapshot: new sessions are added to both sessions_ and the
    // authoritative map; stale entries in sessions_ are harmless because
    // shared_ptr prevents use-after-free and session IDs are never reused.
    {
        AutoLocker al2(sock->priv->socket->priv->quic_sessions_lock);
        sessions_ = sock->priv->socket->priv->quic_sessions;
    }

    // Start by waiting for QUIC packets
    qcs_state = sessions_.empty() ? QCS::HANDSHAKE_RECV : QCS::READING;
}

const char* SocketQuicServerPollOperation::getStateImpl() const {
    switch (qcs_state) {
        case QCS::NONE: return "initializing";
        case QCS::HANDSHAKE_RECV: return "handshake-recv";
        case QCS::READING: return "reading";
        case QCS::REQUEST_READY: return "request-ready";
        case QCS::HEADERS_READY: return "headers-ready";
        default: return "unknown";
    }
}

void SocketQuicServerPollOperation::setHeadersOnly(bool v) {
    headers_only_ = v;
    for (auto& [id, session] : sessions_) {
        session->setHeadersOnlyMode(v);
    }
}

void SocketQuicServerPollOperation::setMaxRequestBodySize(int64_t size) {
    max_request_body_size_ = size;
    for (auto& [id, session] : sessions_) {
        session->setMaxRequestBodySize(size);
    }
}

QoreHashNode* SocketQuicServerPollOperation::checkHeadersOnlyDispatch(bool& handled,
        ExceptionSink* xsink) {
    handled = false;
    if (!headers_only_) {
        return nullptr;
    }
    // Iterate sessions to find one with a headers-ready stream
    for (auto& [id, session] : sessions_) {
        auto stream = session->takeHeadersReadyStreamCopy();
        if (stream) {
            handled = true;
            cached_stream_ = std::make_unique<CachedStream>();
            cached_stream_->session_id = session->getSessionId();
            cached_stream_->stream = std::move(stream);
            cached_stream_->session = session;
            qcs_state = QCS::HEADERS_READY;
            sock->priv->clearNonBlock();
            set_non_block = false;
            return nullptr;  // goal reached
        }
    }
    return nullptr;
}

QoreValue SocketQuicServerPollOperation::getOutput() const {
    if (!cached_stream_) {
        return QoreValue();
    }

    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), nullptr);

    h->setKeyValue("session_id", cached_stream_->session_id, nullptr);
    h->setKeyValue("method", new QoreStringNode(cached_stream_->stream->method), nullptr);
    h->setKeyValue("path", new QoreStringNode(cached_stream_->stream->path), nullptr);
    h->setKeyValue("stream_id", cached_stream_->stream->stream_id, nullptr);

    if (!cached_stream_->stream->error_message.empty()) {
        h->setKeyValue("err", new QoreStringNode("QUIC-BODY-TOO-LARGE"), nullptr);
        h->setKeyValue("desc", new QoreStringNode(cached_stream_->stream->error_message), nullptr);
    }

    if (!cached_stream_->stream->authority.empty()) {
        h->setKeyValue("authority", new QoreStringNode(cached_stream_->stream->authority), nullptr);
    }
    if (!cached_stream_->stream->scheme.empty()) {
        h->setKeyValue("scheme", new QoreStringNode(cached_stream_->stream->scheme), nullptr);
    }

    // Extract peer address from the session that owns this stream
    if (cached_stream_->session) {
        struct sockaddr_storage raddr;
        socklen_t raddr_len;
        cached_stream_->session->getRemoteAddrCopy(raddr, raddr_len);
        char addr_buf[INET6_ADDRSTRLEN];
        int peer_port = 0;
        if (raddr.ss_family == AF_INET) {
            const auto* sin = reinterpret_cast<const struct sockaddr_in*>(&raddr);
            inet_ntop(AF_INET, &sin->sin_addr, addr_buf, sizeof(addr_buf));
            peer_port = ntohs(sin->sin_port);
        } else if (raddr.ss_family == AF_INET6) {
            const auto* sin6 = reinterpret_cast<const struct sockaddr_in6*>(&raddr);
            inet_ntop(AF_INET6, &sin6->sin6_addr, addr_buf, sizeof(addr_buf));
            peer_port = ntohs(sin6->sin6_port);
        } else {
            snprintf(addr_buf, sizeof(addr_buf), "unknown");
        }
        h->setKeyValue("peer_address", new QoreStringNode(addr_buf), nullptr);
        h->setKeyValue("peer_port", peer_port, nullptr);
    }

    // RFC 9220: Add :protocol pseudo-header for extended CONNECT
    if (!cached_stream_->stream->connect_protocol.empty()) {
        h->setKeyValue("connect_protocol", new QoreStringNode(cached_stream_->stream->connect_protocol), nullptr);
    }

    // Headers
    // RFC 9114 section 4.2.1: multiple header values must be concatenated for
    // compatibility with HTTP/1.1 processing code.  Cookie uses "; " (RFC 9114
    // section 4.2.1), other headers use ", " (RFC 7230 section 3.2.6).
    // set-cookie is an exception and must remain as a list (RFC 7230 section 3.2.2).
    ReferenceHolder<QoreHashNode> headers(new QoreHashNode(autoTypeInfo), nullptr);
    for (const auto& hdr : cached_stream_->stream->headers) {
        if (hdr.second.size() == 1) {
            headers->setKeyValue(hdr.first.c_str(), new QoreStringNode(hdr.second[0]), nullptr);
        } else if (hdr.first == "cookie") {
            // RFC 9114 section 4.2.1: concatenate cookie values with "; "
            QoreStringNode* combined = new QoreStringNode;
            for (size_t i = 0; i < hdr.second.size(); ++i) {
                if (i > 0) {
                    combined->concat("; ");
                }
                combined->concat(hdr.second[i]);
            }
            headers->setKeyValue("cookie", combined, nullptr);
        } else if (hdr.first == "set-cookie") {
            // set-cookie must not be combined (RFC 7230 section 3.2.2)
            ReferenceHolder<QoreListNode> values(new QoreListNode(autoTypeInfo), nullptr);
            for (const auto& v : hdr.second) {
                values->push(new QoreStringNode(v), nullptr);
            }
            headers->setKeyValue("set-cookie", values.release(), nullptr);
        } else {
            // RFC 7230 section 3.2.6: concatenate with ", "
            QoreStringNode* combined = new QoreStringNode;
            for (size_t i = 0; i < hdr.second.size(); ++i) {
                if (i > 0) {
                    combined->concat(", ");
                }
                combined->concat(hdr.second[i]);
            }
            headers->setKeyValue(hdr.first.c_str(), combined, nullptr);
        }
    }
    // RFC 9220: Add :protocol to headers hash (parallel to HTTP/2 for handler compatibility)
    if (!cached_stream_->stream->connect_protocol.empty()) {
        headers->setKeyValue(":protocol",
            new QoreStringNode(cached_stream_->stream->connect_protocol), nullptr);
    }
    h->setKeyValue("headers", headers.release(), nullptr);

    // Body
    if (!cached_stream_->stream->body.empty()) {
        SimpleRefHolder<BinaryNode> body(new BinaryNode());
        body->append(cached_stream_->stream->body.data(), cached_stream_->stream->body.size());
        h->setKeyValue("body", body.release(), nullptr);
    }

    // Indicate headers-only dispatch (stream still in map for incremental reading)
    if (qcs_state == QCS::HEADERS_READY) {
        h->setKeyValue("hdr", true, nullptr);
        h->setKeyValue("headers_end_stream", cached_stream_->stream->headers_end_stream, nullptr);
    }

    // Consume the cached stream so subsequent calls return NOTHING
    cached_stream_.reset();

    return h.release();
}

int SocketQuicServerPollOperation::processTimersAndSendAll(
    ngtcp2_tstamp& min_expiry, ExceptionSink* xsink) {
    int fd = sock->priv->socket->getSocket();
    min_expiry = UINT64_MAX;
    int result = 0;

    for (auto& entry : sessions_) {
        auto& session = entry.second;
        if (session->isClosed()) {
            // Clear stale pending_write flag to prevent POLLOUT spin.
            // A closed session won't generate any more packets, but
            // pending_write_ may still be set from the last successful
            // ngtcp2_conn_read_pkt() call before the session closed.
            // Without this, hasPendingWrite() returns true, events include
            // POLLOUT, and UDP (always writable) causes a busy-loop.
            session->clearPendingWrite();
            continue;
        }

        // Coalesced: timer check + packet generation + next expiry (1 lock per session)
        pkt_batch_.clear();
        auto tw = session->processTimerAndWrite(pkt_batch_, xsink);
        if (tw.error) {
            return -1;
        }
        if (tw.next_expiry < min_expiry) {
            min_expiry = tw.next_expiry;
        }

        if (pkt_batch_.empty()) {
            continue;
        }

        // Send QUIC packets as UDP datagrams to the session's remote address.
        // Copy the address under the session lock to avoid reading a partially-
        // updated address if migration changes it concurrently.
        struct sockaddr_storage peer_addr;
        socklen_t peer_addrlen;
        session->getRemoteAddrCopy(peer_addr, peer_addrlen);

        // Copy per-session local address for source IP pinning (multi-homed servers)
        struct sockaddr_storage local_addr;
        socklen_t local_addrlen;
        session->getLocalAddrCopy(local_addr, local_addrlen);

        int sent = sendQuicPacketsBatch(fd, pkt_batch_,
            reinterpret_cast<const struct sockaddr*>(&peer_addr), peer_addrlen,
            reinterpret_cast<const struct sockaddr*>(&local_addr), local_addrlen);
        printd(5, "processTimersAndSendAll() session %lld: %d/%d packets sent\n",
            (long long)entry.first, sent, (int)pkt_batch_.size());
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                result = SOCK_POLLOUT;
            } else {
                xsink->raiseErrnoException("QUIC-SEND-ERROR", errno, "sendto/sendmmsg() failed");
                return -1;
            }
        } else if (sent > 0 && sent < pkt_batch_.size()) {
            // Log partial send; unsent packets will be retransmitted by ngtcp2 via
            // PTO timers.  Server uses a shared UDP socket with per-session addressing,
            // so retaining unsent packets per-session would require a per-session send
            // queue — not worth the complexity since ngtcp2 retransmission handles it.
            printd(1, "SocketQuicServerPollOperation::processTimersAndSendAll(): partial QUIC send: "
                "%d/%d packets for session %lld\n", sent, pkt_batch_.size(),
                (long long)entry.first);
            result = SOCK_POLLOUT;
        }
    }

    return result;
}

int SocketQuicServerPollOperation::recvAndProcessPacket(ExceptionSink* xsink, QuicSession** target_out) {
    // Thread-local to reduce stack pressure (see QuicCommon.h for rationale)
    static thread_local struct sockaddr_storage src_addr;
    static thread_local uint8_t cmsg_buf[QUIC_CMSG_BUF_SIZE];
    socklen_t src_addrlen = sizeof(src_addr);

    int fd = sock->priv->socket->getSocket();
    size_t cmsg_len = sizeof(cmsg_buf);
    ssize_t nread = recvQuicPacket(fd, recv_buf_, sizeof(recv_buf_), 0,
                                    reinterpret_cast<struct sockaddr*>(&src_addr), &src_addrlen,
                                    cmsg_buf, &cmsg_len);
    if (nread < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return SOCK_POLLIN;  // no data available
        }
        if (errno == EMSGSIZE) {
            return 0;  // truncated datagram discarded; continue draining
        }
        xsink->raiseErrnoException("QUIC-RECV-ERROR", errno, "recvmsg() failed");
        return -1;
    }

    if (nread == 0) {
        return 0;
    }

    // Start from cached local address (has correct family + port), then
    // overwrite the IP from pktinfo for actual per-packet destination
    struct sockaddr_storage local_addr;
    socklen_t local_addrlen = local_addrlen_;
    memcpy(&local_addr, &local_addr_, local_addrlen_);
    extractPktinfoAddr(cmsg_buf, cmsg_len, local_addr_.ss_family, &local_addr);

    // Try to dispatch via DCID to an existing session.  The dispatcher returns a
    // session ID, which must be pinned through the owning map: a session that was
    // reaped (cleanupClosedSessions()) or aborted while another thread still held
    // a transient shared_ptr can leave stale CID entries behind, and its
    // destruction may be pending on that other thread — the map miss below is
    // what makes such late packets safe to drop.
    QoreDatagramDispatcher& dispatcher = sock->priv->socket->priv->getQuicDispatcher();
    int64_t target_sid = dispatcher.dispatch(recv_buf_, static_cast<size_t>(nread));

    // Pin the target session for the whole packet-processing call
    std::shared_ptr<QuicSession> pinned;
    QuicSession* target_session = nullptr;
    if (target_sid >= 0) {
        auto sit = sessions_.find(target_sid);
        if (sit == sessions_.end()) {
            // Stale CID for a removed session — drop the packet
            return 0;
        }
        pinned = sit->second;
        target_session = pinned.get();
    } else {
        // Unknown DCID — check if this is an Initial packet for a new connection
        ngtcp2_pkt_hd hdr;
        int rv = ngtcp2_accept(&hdr, recv_buf_, static_cast<size_t>(nread));
        if (rv != 0) {
            // Not a valid Initial packet, ignore
            return 0;
        }

        if (!sock->priv->cert || !sock->priv->pk) {
            xsink->raiseException("QUIC-ERROR",
                "server certificate and private key must be set on the socket "
                "before accepting QUIC connections");
            return -1;
        }

        // Enforce maximum session count to prevent memory exhaustion from
        // a flood of Initial packets
        if (sessions_.size() >= MAX_QUIC_SERVER_SESSIONS) {
            qore_async_io_log(QORE_LOG_LEVEL_WARN,
                "QUIC: max server sessions (%zu) reached, dropping new connection",
                MAX_QUIC_SERVER_SESSIONS);
            return 0;
        }

        // Get or create the shared server SSL_CTX for session ticket key continuity (0-RTT)
        SSL_CTX* shared_ctx = sock->priv->socket->priv->getOrCreateQuicServerSslCtx(
            sock->priv->cert, sock->priv->pk, xsink);
        if (*xsink) {
            QoreStringValueHelper err_str(xsink->getExceptionErr());
            QoreStringValueHelper desc_str(xsink->getExceptionDesc());
            qore_async_io_log(QORE_LOG_LEVEL_WARN,
                "QUIC: failed to create shared server SSL_CTX: %s: %s",
                *err_str ? err_str->c_str() : "unknown",
                *desc_str ? desc_str->c_str() : "unknown");
            xsink->clear();
            return 0;
        }

        // Create a new session with dispatcher for CID registration and shared SSL_CTX.
        // Pass through the socket's ssl_verify_mode to enable mTLS when configured.
        auto new_session = QuicSession::createServer(
            sock->priv->socket->priv, xsink, &hdr,
            sock->priv->cert, sock->priv->pk,
            reinterpret_cast<const struct sockaddr*>(&local_addr), local_addrlen,
            reinterpret_cast<const struct sockaddr*>(&src_addr), src_addrlen,
            &dispatcher, shared_ctx,
            sock->priv->socket->priv->ssl_verify_mode,
            sock->priv->socket->priv->ssl_accept_all_certs);
        if (*xsink || !new_session) {
            // Log and continue — a single client's failed handshake should not
            // abort the server for all other clients
            QoreStringValueHelper err_str(xsink->getExceptionErr());
            QoreStringValueHelper desc_str(xsink->getExceptionDesc());
            qore_async_io_log(QORE_LOG_LEVEL_WARN,
                "QUIC: failed to create server session: %s: %s",
                *err_str ? err_str->c_str() : "unknown",
                *desc_str ? desc_str->c_str() : "unknown");
            xsink->clear();
            return 0;
        }

        // Store in both local and qore_socket_private session maps
        sessions_[new_session->getSessionId()] = new_session;
        sock->priv->socket->priv->addQuicSession(new_session);
        // Propagate headers-only mode to new sessions
        if (headers_only_) {
            new_session->setHeadersOnlyMode(true);
        }
        // Propagate max request body size to new sessions
        if (max_request_body_size_ > 0) {
            new_session->setMaxRequestBodySize(max_request_body_size_);
        }

        target_session = new_session.get();
    }

    // Return the target session to the caller for targeted stream checking
    if (target_out) {
        *target_out = target_session;
    }

    // Build path for ngtcp2
    // ngtcp2_conn_read_pkt copies the path addresses; stack-local is safe
    ngtcp2_path path;
    path.local.addr = reinterpret_cast<ngtcp2_sockaddr*>(&local_addr);
    path.local.addrlen = local_addrlen;
    path.remote.addr = reinterpret_cast<ngtcp2_sockaddr*>(&src_addr);
    path.remote.addrlen = src_addrlen;

    int rv = target_session->readPacket(recv_buf_, static_cast<size_t>(nread), path, xsink);
    if (*xsink) {
        // A single session's packet read failure (e.g., TLS handshake error when
        // client cert is required but not provided) should not abort the entire
        // server listener.  Log the error and send a CONNECTION_CLOSE to the
        // client so it gets a clean error instead of timing out.
        QoreStringValueHelper err_str(xsink->getExceptionErr());
        QoreStringValueHelper desc_str(xsink->getExceptionDesc());
        qore_async_io_log(QORE_LOG_LEVEL_WARN,
            "QUIC: readPacket() failed for session %lld: %s: %s",
            (long long)target_session->getSessionId(),
            *err_str ? err_str->c_str() : "unknown",
            *desc_str ? desc_str->c_str() : "unknown");
        xsink->clear();

        // Send CONNECTION_CLOSE frame so the client gets an immediate error
        // rather than waiting for a timeout.  The ngtcp2 connection has the
        // TLS alert set from the failed SSL_do_handshake().
        {
            uint8_t close_buf[1280];  // minimum QUIC packet size
            ssize_t close_len = target_session->writeConnectionClose(
                close_buf, sizeof(close_buf));
            if (close_len > 0) {
                struct sockaddr_storage remote_addr;
                socklen_t remote_addrlen;
                target_session->getRemoteAddrCopy(remote_addr, remote_addrlen);
                int fd = sock->priv->socket->getSocket();
                (void)sendto(fd, close_buf, static_cast<size_t>(close_len), 0,
                    reinterpret_cast<const struct sockaddr*>(&remote_addr),
                    remote_addrlen);
            }
        }

        // Clear target_out so the caller does not use this failed session
        if (target_out) {
            *target_out = nullptr;
        }
        return 0;
    }

    // If handshake just completed and HTTP/3 not yet set up, do it now
    if (target_session->isHandshakeComplete() && !target_session->isHttp3Ready()) {
        target_session->setupHttp3(xsink);
        if (*xsink) {
            // Log and continue — a single session's HTTP/3 setup failure should not
            // halt the server for all other clients
            QoreStringValueHelper err_str(xsink->getExceptionErr());
            QoreStringValueHelper desc_str(xsink->getExceptionDesc());
            qore_async_io_log(QORE_LOG_LEVEL_WARN,
                "QUIC: setupHttp3() failed for session %lld: %s: %s",
                (long long)target_session->getSessionId(),
                *err_str ? err_str->c_str() : "unknown",
                *desc_str ? desc_str->c_str() : "unknown");
            xsink->clear();
        }
    }

    return 0;
}

// Remove sessions that are closed and have no pending completed streams.
// This also removes CIDs from the dispatcher — explicitly, via
// unregisterFromDispatcher(), NOT by relying on ~QuicSession(): the destructor
// runs on whatever thread drops the last shared_ptr, and a foreign thread's
// transient reference can keep the session alive past this reap, which would
// leave its CIDs routable to a removed session in the meantime.
// Called periodically from continuePoll() to avoid unbounded session growth.
// Note: idle sessions are handled by ngtcp2's built-in idle timeout
// (QUIC_MAX_IDLE_TIMEOUT), which marks them as closed after inactivity.
//
// We also require hasDispatchedStreams() == false before removing the
// session: a handler thread that received a headers-only dispatched stream
// looks the session up by session_id via qore_socket_private::getQuicSession
// when it calls readQuicStreamDataBlock / cleanupQuicStream, so the session
// must stay in the socket's session map until every dispatched stream has
// been erased (cleanupStream / resetStream / cancelStream /
// takeCompletedStream).  Without this guard, a GET-with-body over H3 where
// the client tears the QUIC connection down immediately after the response
// would race: isClosed() flips true before the handler thread reads the
// body, cleanupClosedSessions erases the session from the socket map, then
// the handler's readQuicStreamDataBlock throws QUIC-ERROR "no QUIC session
// with id N on this socket".
void SocketQuicServerPollOperation::cleanupClosedSessions() {
    auto it = sessions_.begin();
    while (it != sessions_.end()) {
        if (it->second->isClosed()
                && !it->second->hasCompletedStreams()
                && !it->second->hasDispatchedStreams()) {
            // Report the close before erasing so the persistent-session teardown
            // path is not lost when this is the only place the session is reaped
            // (e.g. a session that closed without any reset/abandon event).
            if (it->second->reportCloseIfNew()) {
                lifecycle_closed_sessions_.push_back(it->first);
            }
            // Unregister the session's CIDs at reap time: destruction runs on
            // whatever thread drops the last shared_ptr (a foreign thread's
            // transient ref — e.g. cancelQuicStreamRead() during a service
            // stop — can hold the session past this reap), so deferring CID
            // cleanup to the destructor leaves late packets routable to the
            // removed session in the meantime
            it->second->unregisterFromDispatcher();
            sock->priv->socket->priv->removeQuicSession(it->first);
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

void SocketQuicServerPollOperation::collectSessionLifecycleEvents() {
    // Called on the I/O thread with sock->priv->m held.  Folds two cheap
    // per-session checks: drain peer-reset reports and detect newly-closed
    // sessions (closed-but-not-yet-reaped sessions are reported here; reaped
    // ones are reported in cleanupClosedSessions()).  reportCloseIfNew() is
    // one-shot per session, so the two paths never double-report.
    for (auto& entry : sessions_) {
        std::vector<int64_t> resets = entry.second->takePendingPeerResetReports();
        for (int64_t stream_id : resets) {
            lifecycle_peer_resets_.emplace_back(entry.first, stream_id);
        }
        if (entry.second->reportCloseIfNew()) {
            ASYNC_IO_TRACE("SocketQuicServerPollOperation::collectSessionLifecycleEvents "
                "session closed=%lld\n", (long long)entry.first);
            lifecycle_closed_sessions_.push_back(entry.first);
        }
    }
}

QoreHashNode* SocketQuicServerPollOperation::continuePoll(ExceptionSink* xsink) {
    AutoLocker al(sock->priv->m);

    if (!sock->priv->socket->priv->isOpen()) {
        xsink->raiseException("QUIC-POLL-ERROR", "socket closed during poll operation");
        return nullptr;
    }

    while (true) {
        switch (qcs_state) {
            case QCS::HANDSHAKE_RECV: {
                // Initial state: drain packets until we have at least one session
                while (true) {
                    int rv = recvAndProcessPacket(xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (rv == SOCK_POLLIN) {
                        break;  // EAGAIN — no more data
                    }
                }

                if (sessions_.empty()) {
                    // Didn't get a valid Initial packet yet
                    return getSocketPollInfoHash(xsink, SOCK_POLLIN);
                }

                // We have at least one session; coalesced timer + write + send
                {
                    ngtcp2_tstamp min_expiry;
                    int rv = processTimersAndSendAll(min_expiry, xsink);
                    if (*xsink) {
                        return nullptr;
                    }

                    // Move to the main reading state
                    qcs_state = QCS::READING;

                    // Compute QUIC-aware poll timeout from handshake timers
                    // (retransmission PTO may already be pending from handshake)
                    QoreHashNode* poll_info = getSocketPollInfoHash(xsink, SOCK_POLLIN);
                    if (poll_info) {
                        setPollTimeoutFromExpiry(poll_info, min_expiry, xsink);
                    }
                    return poll_info;
                }
            }

            case QCS::READING: {
                // Drain all available packets from the socket buffer FIRST.
                // This ensures QUIC DATAGRAM frames (which may arrive in the
                // same batch as HEADERS) are buffered in datagram_queues_
                // before any handler is dispatched via headers-only or
                // completed-stream checks below.
                while (true) {
                    QuicSession* target = nullptr;
                    int rv = recvAndProcessPacket(xsink, &target);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (rv == SOCK_POLLIN) {
                        break;  // EAGAIN — no more data
                    }
                }

                // Surface per-session lifecycle events (peer stream resets and
                // session closes) seen while draining packets above, so the
                // controller can tear down a persistent dedicated handler thread
                // on a multiplexed connection.  Done before the early-return
                // completed-stream checks so it runs on every READING poll.
                collectSessionLifecycleEvents();

                // Headers-only mode: check for streams with HEADERS complete
                // (either from this drain or a previous cycle)
                {
                    bool handled = false;
                    QoreHashNode* rv = checkHeadersOnlyDispatch(handled, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (handled) {
                        return rv;
                    }
                }

                // Check all sessions for completed streams (after draining)
                for (auto& entry : sessions_) {
                    auto& session = entry.second;
                    if (session->hasCompletedStreams()) {
                        auto stream = session->takeCompletedStream();
                        cached_stream_ = std::make_unique<CachedStream>();
                        cached_stream_->session_id = session->getSessionId();
                        cached_stream_->stream = std::move(stream);
                        cached_stream_->session = session;
                        qcs_state = QCS::REQUEST_READY;
                        sock->priv->clearNonBlock();
                        set_non_block = false;
                        return nullptr;  // goal reached
                    }
                }

                // Process InputStreams for all sessions with active streams
                for (auto& entry : sessions_) {
                    if (entry.second->hasActiveStreamInputStreams()) {
                        printd(5, "SocketQuicServerPollOperation::continuePoll() session %lld has active InputStreams\n",
                            (long long)entry.first);
                        entry.second->processStreamInputStreams(xsink);
                        if (*xsink) {
                            return nullptr;
                        }
                    }
                }

                // Coalesced: timer + write + send for all sessions (single pass)
                {
                    ngtcp2_tstamp min_expiry;
                    int srv = processTimersAndSendAll(min_expiry, xsink);
                    if (*xsink) {
                        return nullptr;
                    }

                    // Check again for completed streams (after send)
                    for (auto& entry : sessions_) {
                        auto& session = entry.second;
                        if (session->hasCompletedStreams()) {
                            auto stream = session->takeCompletedStream();
                            cached_stream_ = std::make_unique<CachedStream>();
                            cached_stream_->session_id = session->getSessionId();
                            cached_stream_->session = session;
                            cached_stream_->stream = std::move(stream);
                            qcs_state = QCS::REQUEST_READY;
                            sock->priv->clearNonBlock();
                            set_non_block = false;
                            return nullptr;  // goal reached
                        }
                    }

                    // Periodically clean up closed sessions (amortized O(n/CLEANUP_INTERVAL))
                    if (++cleanup_counter_ >= CLEANUP_INTERVAL) {
                        cleanup_counter_ = 0;
                        cleanupClosedSessions();
                    }

                    // Register for POLLIN always; add POLLOUT only when the UDP
                    // socket itself has backpressure (sendto returned EAGAIN).
                    // Do NOT add POLLOUT for hasPendingWrite() — UDP sockets are
                    // always writable, so POLLOUT would cause a busy-loop spinning
                    // between continuePoll() and poll() while waiting for ACKs to
                    // open the congestion window.  POLLIN (for ACKs) plus the QUIC
                    // timer timeout (for PTO retransmission) is sufficient.
                    int events = SOCK_POLLIN;
                    if (srv == SOCK_POLLOUT) {
                        events |= SOCK_POLLOUT;
                    }

                    // Collect extra fds from all sessions with active pollable InputStreams
                    std::vector<std::pair<int, int>> extra_fds;
                    bool has_active_input_streams = false;
                    for (auto& entry : sessions_) {
                        if (entry.second->hasActiveStreamInputStreams()) {
                            has_active_input_streams = true;
                        }
                        entry.second->getExtraFds(extra_fds);
                    }

                    // Compute QUIC-aware poll timeout hint from timer expiries
                    QoreHashNode* poll_info;
                    if (!extra_fds.empty()) {
                        poll_info = getSocketPollInfoHash(xsink, events, extra_fds);
                    } else {
                        poll_info = getSocketPollInfoHash(xsink, events);
                    }
                    if (poll_info) {
                        setPollTimeoutFromExpiry(poll_info, min_expiry, xsink);
                        // If there are non-pollable active InputStreams (no extra
                        // fds to watch), use a small poll timeout so the I/O thread
                        // reads from them periodically without busy-looping.
                        if (has_active_input_streams && extra_fds.empty()) {
                            QoreValue ptv = poll_info->getKeyValue("poll_timeout_ms");
                            int64_t current_ms = ptv.isNullOrNothing() ? INT64_MAX
                                : ptv.getAsBigInt();
                            if (current_ms > 1) {
                                poll_info->setKeyValue("poll_timeout_ms",
                                    static_cast<int64_t>(1), xsink);
                            }
                        }
                    }
                    return poll_info;
                }
            }

            case QCS::REQUEST_READY:
            case QCS::HEADERS_READY: {
                // Already reached goal; if called again, go back to reading
                // Flush pending writes (ACKs, handshake responses, flow control)
                // for ALL sessions before checking for more streams.  Without
                // this, handshake responses for sessions created during the
                // initial packet drain are delayed until the state transitions
                // to READING, stalling concurrent client connections.
                // NOTE: do NOT drain packets here — the socket is in blocking
                // mode (restored by checkHeadersOnlyDispatch), so recvfrom()
                // would block.  Draining happens in READING after the
                // non-blocking transition.
                {
                    ngtcp2_tstamp min_expiry;
                    if (processTimersAndSendAll(min_expiry, xsink) < 0 || *xsink) {
                        return nullptr;
                    }
                }
                // Check for headers-only dispatch first
                if (headers_only_) {
                    for (auto& entry : sessions_) {
                        auto stream = entry.second->takeHeadersReadyStreamCopy();
                        if (stream) {
                            cached_stream_ = std::make_unique<CachedStream>();
                            cached_stream_->session_id = entry.second->getSessionId();
                            cached_stream_->stream = std::move(stream);
                            cached_stream_->session = entry.second;
                            qcs_state = QCS::HEADERS_READY;
                            return nullptr;
                        }
                    }
                }
                // Check all sessions for more completed streams
                for (auto& entry : sessions_) {
                    auto& session = entry.second;
                    if (session->hasCompletedStreams()) {
                        auto stream = session->takeCompletedStream();
                        cached_stream_ = std::make_unique<CachedStream>();
                        cached_stream_->session_id = session->getSessionId();
                        cached_stream_->stream = std::move(stream);
                        cached_stream_->session = session;
                        qcs_state = QCS::REQUEST_READY;
                        return nullptr;
                    }
                }
                cached_stream_.reset();
                qcs_state = QCS::READING;
                if (!set_non_block) {
                    if (sock->priv->setNonBlock(xsink)) {
                        return nullptr;
                    }
                    set_non_block = true;
                    // Set OS-level non-blocking mode for QUIC raw UDP I/O
                    if (sock->priv->socket->priv->set_non_blocking(true, xsink)) {
                        sock->priv->clearNonBlock();
                        set_non_block = false;
                        return nullptr;
                    }
                }
                continue;
            }

            default:
                xsink->raiseException("QUIC-POLL-ERROR", "invalid server poll state: %d", static_cast<int>(qcs_state));
                return nullptr;
        }
    }
}

// ============================================================
// SocketQuicSendResponsePollOperation
// ============================================================

SocketQuicSendResponsePollOperation::SocketQuicSendResponsePollOperation(
    ExceptionSink* xsink, QoreSocketObject* sock,
    int64_t session_id, int64_t stream_id, int status_code,
    const QoreHashNode* headers,
    const AbstractQoreNode* body)
    : SocketPollSocketOperationBase(sock), stream_id_(stream_id), status_code_(status_code) {
    AutoLocker al(sock->priv->m);

    // Validate socket
    if (sock->priv->checkOpen(xsink)) {
        return;
    }

    // Get the QUIC session by session_id from the socket
    quic_session = sock->priv->socket->priv->getQuicSession(session_id);
    if (!quic_session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return;
    }

    // Convert headers hash to std::map
    if (headers) {
        ConstHashIterator hi(headers);
        while (hi.next()) {
            QoreStringValueHelper val(hi.get());
            header_map_[hi.getKey()] = val->c_str();
        }
    }

    // Copy body data so session mutation can be deferred to continuePoll().
    if (body) {
        qore_type_t body_type = body->getType();
        if (body_type == NT_BINARY) {
            const BinaryNode* b = static_cast<const BinaryNode*>(body);
            if (b->size()) {
                body_ = new BinaryNode;
                body_->append(b->getPtr(), b->size());
            }
        } else if (body_type == NT_STRING) {
            const QoreStringNode* str = static_cast<const QoreStringNode*>(body);
            if (str->size()) {
                body_ = new BinaryNode;
                body_->append(str->c_str(), str->size());
            }
        }
    }

    // Set non-blocking guard flag on QoreSocketObject
    if (sock->priv->setNonBlock(xsink)) {
        return;
    }
    set_non_block = true;

    // Set OS-level non-blocking mode on the fd — required because QUIC uses
    // raw recvfrom()/sendto() on the UDP socket
    if (sock->priv->socket->priv->set_non_blocking(true, xsink)) {
        sock->priv->clearNonBlock();
        set_non_block = false;
        return;
    }

    // Copy the remote peer address from the QUIC session for sendto()
    quic_session->getRemoteAddrCopy(peer_addr_, peer_addrlen_);

    // Copy per-session local address for source IP pinning (multi-homed servers)
    quic_session->getLocalAddrCopy(send_local_addr_, send_local_addrlen_);

    // Cache local address (family + port); per-packet destination IP is
    // extracted from pktinfo control messages in recvAndProcessPackets()
    int fd = sock->priv->socket->getSocket();
    local_addrlen_ = sizeof(local_addr_);
    if (getsockname(fd, reinterpret_cast<struct sockaddr*>(&local_addr_), &local_addrlen_) < 0) {
        xsink->raiseErrnoException("QUIC-ERROR", errno, "getsockname() failed in send-response setup");
        sock->priv->clearNonBlock();
        set_non_block = false;
        return;
    }
    if (enableQuicPktinfo(fd, local_addr_.ss_family) < 0) {
        printd(0, "SocketQuicSendResponsePollOperation: enableQuicPktinfo() failed: errno=%d (%s)\n",
            errno, strerror(errno));
    }

    qcs_state = QCS::NONE;
}

const char* SocketQuicSendResponsePollOperation::getStateImpl() const {
    switch (qcs_state) {
        case QCS::NONE: return "initializing";
        case QCS::SENDING: return "sending";
        case QCS::FLUSHING: return "flushing";
        case QCS::SENT: return "sent";
        default: return "unknown";
    }
}

int SocketQuicSendResponsePollOperation::sendPendingPackets(
    ngtcp2_tstamp& next_expiry, ExceptionSink* xsink) {
    // Refresh cached peer address if path migration has occurred.
    //
    // IMPORTANT: SINGLE-THREADED ASSUMPTION.  This check-then-copy sequence is safe
    // ONLY because this function runs exclusively in the single-threaded server poll
    // loop.  If the server I/O model ever becomes multi-threaded, the hasPathMigrated()
    // check, getRemoteAddrCopy(), and clearPathMigrated() calls must be made atomic
    // (e.g. via a combined getAndClearMigratedAddr() method under the session lock).
    //
    // hasPathMigrated() is a lock-free atomic check (near-zero cost on the common
    // non-migration path).  When migration is detected, getRemoteAddrCopy() acquires
    // the session lock to read the new address atomically.  The migration_gen_ counter
    // provides eventual consistency — if a second migration occurs between check and
    // copy, the flag will be set again and the next call will refresh the cache.
    if (quic_session->hasPathMigrated()) {
        quic_session->getRemoteAddrCopy(peer_addr_, peer_addrlen_);
        quic_session->getLocalAddrCopy(send_local_addr_, send_local_addrlen_);
        quic_session->clearPathMigrated();
    }

    // Coalesced timer check + packet generation under a single lock
    auto result = quic_session->processTimerAndWrite(pkt_batch_, xsink);
    if (result.error) {
        pkt_batch_.clear();
        return -1;
    }
    next_expiry = result.next_expiry;

    if (pkt_batch_.empty()) {
        return 0;
    }

    // Re-check migration after packet generation: writePacketsLocked() detects
    // new paths from ngtcp2 output (e.g. server-side passive migration) and sets
    // path_migrated_.  Without this second check, the first post-migration batch
    // (including PATH_RESPONSE) would be sent to the old client address.
    if (quic_session->hasPathMigrated()) {
        quic_session->getRemoteAddrCopy(peer_addr_, peer_addrlen_);
        quic_session->getLocalAddrCopy(send_local_addr_, send_local_addrlen_);
        quic_session->clearPathMigrated();
    }

    int fd = sock->priv->socket->getSocket();

    int sent = sendQuicPacketsBatch(fd, pkt_batch_,
        reinterpret_cast<const struct sockaddr*>(&peer_addr_), peer_addrlen_,
        reinterpret_cast<const struct sockaddr*>(&send_local_addr_), send_local_addrlen_);
    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return SOCK_POLLOUT;
        }
        pkt_batch_.clear();
        xsink->raiseErrnoException("QUIC-SEND-ERROR", errno, "sendto/sendmmsg() failed");
        return -1;
    }
    if (sent > 0 && sent < static_cast<int>(pkt_batch_.size())) {
        printd(1, "SocketQuicSendResponsePollOperation::sendPendingPackets(): partial QUIC send: %d/%d packets\n",
            sent, static_cast<int>(pkt_batch_.size()));
        // Retain unsent packets for the next send cycle instead of dropping them;
        // ngtcp2 would retransmit via PTO timers, but sending them promptly avoids
        // the unnecessary delay and bandwidth overhead of retransmission
        pkt_batch_.removeFront(sent);
        return SOCK_POLLOUT;
    }

    pkt_batch_.clear();
    return 0;
}

int SocketQuicSendResponsePollOperation::recvAndProcessPackets(ExceptionSink* xsink) {
    return recvAndDispatchQuicPackets(
        sock->priv->socket->getSocket(),
        sock->priv->socket->priv->getQuicDispatcher(),
        local_addr_, local_addrlen_,
        recv_buf_, sizeof(recv_buf_), sock->priv->socket->priv, xsink);
}

QoreHashNode* SocketQuicSendResponsePollOperation::continuePoll(ExceptionSink* xsink) {
    AutoLocker al(sock->priv->m);

    if (!sock->priv->socket->priv->isOpen()) {
        xsink->raiseException("QUIC-POLL-ERROR", "socket closed during poll operation");
        return nullptr;
    }

    // Guard against continuePoll() after abort() has reset quic_session
    if (!quic_session) {
        xsink->raiseException("QUIC-POLL-ERROR", "QUIC send-response session has been aborted");
        return nullptr;
    }

    while (true) {
        switch (qcs_state) {
            case QCS::NONE: {
                const void* body_ptr = body_ ? body_->getPtr() : nullptr;
                size_t body_len = body_ ? body_->size() : 0;

                // Submit the response to the HTTP/3 layer on the controller thread.
                int rv = quic_session->submitResponse(stream_id_, status_code_, header_map_, body_ptr, body_len,
                    xsink);
                if (rv && !*xsink) {
                    xsink->raiseException("QUIC-ERROR", "failed to submit HTTP/3 response: rv=%d", rv);
                }
                if (*xsink) {
                    return nullptr;
                }

                // Snapshot migration generation when SENDING state begins.
                send_migration_gen_ = quic_session->getMigrationGen();
                qcs_state = QCS::SENDING;
                continue;
            }

            case QCS::SENDING: {
                // Read any incoming ACKs to update flow control windows
                recvAndProcessPackets(xsink);
                if (*xsink) {
                    return nullptr;
                }

                // Coalesced: timer check + packet generation + next expiry
                ngtcp2_tstamp next_expiry;
                int rv = sendPendingPackets(next_expiry, xsink);
                if (*xsink) {
                    return nullptr;
                }
                if (rv == SOCK_POLLOUT) {
                    // Register for both read (ACKs) and write (more data)
                    QoreHashNode* poll_info = getSocketPollInfoHash(xsink, SOCK_POLLIN | SOCK_POLLOUT);
                    if (poll_info) {
                        setPollTimeoutFromExpiry(poll_info, next_expiry, xsink);
                    }
                    return poll_info;
                }

                // Check if all data has been packetized and sent
                qcs_state = QCS::FLUSHING;
                // Register for both read (ACKs) and write (flush remaining)
                {
                    QoreHashNode* poll_info = getSocketPollInfoHash(xsink, SOCK_POLLIN | SOCK_POLLOUT);
                    if (poll_info) {
                        setPollTimeoutFromExpiry(poll_info, next_expiry, xsink);
                    }
                    return poll_info;
                }
            }

            case QCS::FLUSHING: {
                // Check for peer disconnect before doing more work
                if (quic_session->isClosed()) {
                    qcs_state = QCS::SENT;
                    sock->priv->clearNonBlock();
                    set_non_block = false;
                    return nullptr;
                }

                // Read any incoming ACKs to update flow control windows
                recvAndProcessPackets(xsink);
                if (*xsink) {
                    return nullptr;
                }

                // Coalesced: timer check + packet generation + next expiry
                ngtcp2_tstamp next_expiry;
                int rv = sendPendingPackets(next_expiry, xsink);
                if (*xsink) {
                    return nullptr;
                }
                if (rv == SOCK_POLLOUT) {
                    QoreHashNode* poll_info = getSocketPollInfoHash(xsink, SOCK_POLLIN | SOCK_POLLOUT);
                    if (poll_info) {
                        setPollTimeoutFromExpiry(poll_info, next_expiry, xsink);
                    }
                    return poll_info;
                }

                // Check if there's still more data to send (flow/congestion
                // control may have prevented all data from being written).
                // Wait only for POLLIN (ACKs from peer) to avoid busy-waiting
                // on POLLOUT (UDP sockets are always writable).
                if (quic_session->hasPendingWrite()) {
                    QoreHashNode* poll_info = getSocketPollInfoHash(xsink, SOCK_POLLIN);
                    if (poll_info) {
                        // flow-control-blocked pending data: don't sub-ms tight-poll
                        // on the PTO timer; wait for the unblocking ACK / MAX_STREAM_DATA
                        setPollTimeoutFromExpiry(poll_info, next_expiry, xsink, true);
                    }
                    return poll_info;
                }

                // If a connection migration occurred during this response, data
                // may have been sent to the OLD client address (fire-and-forget
                // UDP) and never received.  ngtcp2's retransmission will recover
                // it on the new path, but only if we keep the poll loop running.
                // Check both bytes_in_flight (connection-level retransmission
                // tracking) and isStreamFullyAcked (definitive stream-close signal)
                // to decide when it's safe to exit.
                if (quic_session->getMigrationGen() != send_migration_gen_) {
                    // Migration detected — wait until either:
                    // (a) stream_close callback has fired (all data ACKed), or
                    // (b) bytes_in_flight drops to 0 (all retransmissions complete)
                    if (!quic_session->isStreamFullyAcked(stream_id_)
                        && quic_session->getBytesInFlight() > 0) {
                        printd(5, "SocketQuicSendResponsePollOperation::continuePoll() "
                            "FLUSHING stream_id=" QLLD " migration detected, "
                            "waiting for ACKs (bytes_in_flight=%" PRIu64 ")\n",
                            stream_id_, quic_session->getBytesInFlight());
                        QoreHashNode* poll_info = getSocketPollInfoHash(xsink, SOCK_POLLIN);
                        if (poll_info) {
                            setPollTimeoutFromExpiry(poll_info, next_expiry, xsink);
                        }
                        return poll_info;
                    }
                    // Clean up the closed_streams_ entry if stream_close fired
                    quic_session->removeClosedStream(stream_id_);
                }

                // All data delivered —
                qcs_state = QCS::SENT;
                sock->priv->clearNonBlock();
                set_non_block = false;
                return nullptr;
            }

            case QCS::SENT:
                if (set_non_block) {
                    sock->priv->clearNonBlock();
                    set_non_block = false;
                }
                return nullptr;

            default:
                xsink->raiseException("QUIC-POLL-ERROR", "invalid send response poll state: %d", static_cast<int>(qcs_state));
                return nullptr;
        }
    }
}

// ============================================================
// SocketQuicSendStreamingResponsePollOperation
// ============================================================

SocketQuicSendStreamingResponsePollOperation::SocketQuicSendStreamingResponsePollOperation(
    ExceptionSink* xsink, QoreSocketObject* sock,
    int64_t session_id, int64_t stream_id, int status_code,
    const QoreHashNode* headers,
    InputStream* input_stream, QoreObject* input_stream_obj, int64 chunk_size)
    : SocketPollSocketOperationBase(sock), session_id(session_id), stream_id(stream_id),
      status_code(status_code),
      input_stream(input_stream), input_stream_obj(input_stream_obj),
      chunk_size(chunk_size > 0 ? chunk_size : 16384),
      is_pollable(false) {
    if (!input_stream->isIoThreadSafe()) {
        xsink->raiseException("QUIC-ERROR", "InputStream is not I/O thread safe");
        return;
    }

    stream_fd = quic_input_stream_pollable_descriptor(input_stream, "QUIC-ERROR", xsink);
    if (*xsink) {
        return;
    }
    is_pollable = stream_fd >= 0;

    AutoLocker al(sock->priv->m);

    // Validate socket
    if (sock->priv->checkOpen(xsink)) {
        return;
    }

    // Get the QUIC session by session_id from the socket
    quic_session = sock->priv->socket->priv->getQuicSession(session_id);
    if (!quic_session) {
        xsink->raiseException("QUIC-ERROR", "no QUIC session with id %lld on this socket",
            (long long)session_id);
        return;
    }

    // Convert headers hash to std::map
    if (headers) {
        ConstHashIterator hi(headers);
        while (hi.next()) {
            QoreStringValueHelper val(hi.get());
            header_map[hi.getKey()] = val->c_str();
        }
    }

    // Set non-blocking guard flag on QoreSocketObject
    if (sock->priv->setNonBlock(xsink)) {
        return;
    }
    set_non_block = true;

    // Set OS-level non-blocking mode on the fd — required because QUIC uses
    // raw recvfrom()/sendto() on the UDP socket
    if (sock->priv->socket->priv->set_non_blocking(true, xsink)) {
        sock->priv->clearNonBlock();
        set_non_block = false;
        return;
    }

    // Copy the remote peer address from the QUIC session for sendto()
    quic_session->getRemoteAddrCopy(peer_addr_, peer_addrlen_);

    // Copy per-session local address for source IP pinning (multi-homed servers)
    quic_session->getLocalAddrCopy(send_local_addr_, send_local_addrlen_);

    // Cache local address
    int fd = sock->priv->socket->getSocket();
    local_addrlen_ = sizeof(local_addr_);
    if (getsockname(fd, reinterpret_cast<struct sockaddr*>(&local_addr_), &local_addrlen_) < 0) {
        xsink->raiseErrnoException("QUIC-ERROR", errno,
            "getsockname() failed in streaming send-response setup");
        sock->priv->clearNonBlock();
        set_non_block = false;
        return;
    }
    if (enableQuicPktinfo(fd, local_addr_.ss_family) < 0) {
        printd(0, "SocketQuicSendStreamingResponsePollOperation: enableQuicPktinfo() failed: "
            "errno=%d (%s)\n", errno, strerror(errno));
    }

    // Thread affinity: QPP wrapper calls unassignThread() after construction
}

const char* SocketQuicSendStreamingResponsePollOperation::getStateImpl() const {
    switch (ss_state) {
        case QCS_SS::SUBMIT_HEADERS: return "submitting-headers";
        case QCS_SS::READ_CHUNK: return "reading-chunk";
        case QCS_SS::SEND_CHUNK: return "sending-chunk";
        case QCS_SS::FLUSH: return "flushing";
        case QCS_SS::RECV_ACK: return "receiving-ack";
        case QCS_SS::DONE: return "done";
        default: return "unknown";
    }
}

int SocketQuicSendStreamingResponsePollOperation::sendPendingPackets(
    ngtcp2_tstamp& next_expiry, ExceptionSink* xsink) {
    // Refresh cached peer address if path migration has occurred.
    // IMPORTANT: same single-threaded assumption as
    // SocketQuicSendResponsePollOperation::sendPendingPackets() — see comment there.
    if (quic_session->hasPathMigrated()) {
        quic_session->getRemoteAddrCopy(peer_addr_, peer_addrlen_);
        quic_session->getLocalAddrCopy(send_local_addr_, send_local_addrlen_);
        quic_session->clearPathMigrated();
    }

    // Coalesced timer check + packet generation under a single lock
    auto result = quic_session->processTimerAndWrite(pkt_batch_, xsink);
    if (result.error) {
        pkt_batch_.clear();
        return -1;
    }
    next_expiry = result.next_expiry;

    if (pkt_batch_.empty()) {
        return 0;
    }

    // Re-check migration after packet generation: writePacketsLocked() detects
    // new paths from ngtcp2 output (e.g. server-side passive migration) and sets
    // path_migrated_.  Without this second check, the first post-migration batch
    // (including PATH_RESPONSE) would be sent to the old client address.
    if (quic_session->hasPathMigrated()) {
        quic_session->getRemoteAddrCopy(peer_addr_, peer_addrlen_);
        quic_session->getLocalAddrCopy(send_local_addr_, send_local_addrlen_);
        quic_session->clearPathMigrated();
    }

    int fd = sock->priv->socket->getSocket();

    int sent = sendQuicPacketsBatch(fd, pkt_batch_,
        reinterpret_cast<const struct sockaddr*>(&peer_addr_), peer_addrlen_,
        reinterpret_cast<const struct sockaddr*>(&send_local_addr_), send_local_addrlen_);
    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return SOCK_POLLOUT;
        }
        pkt_batch_.clear();
        xsink->raiseErrnoException("QUIC-SEND-ERROR", errno, "sendto/sendmmsg() failed");
        return -1;
    }
    if (sent > 0 && sent < static_cast<int>(pkt_batch_.size())) {
        printd(1, "SocketQuicSendStreamingResponsePollOperation::sendPendingPackets(): "
            "partial QUIC send: %d/%d packets\n", sent, static_cast<int>(pkt_batch_.size()));
        pkt_batch_.removeFront(sent);
        return SOCK_POLLOUT;
    }

    pkt_batch_.clear();
    return 0;
}

int SocketQuicSendStreamingResponsePollOperation::recvAndProcessPackets(ExceptionSink* xsink) {
    return recvAndDispatchQuicPackets(
        sock->priv->socket->getSocket(),
        sock->priv->socket->priv->getQuicDispatcher(),
        local_addr_, local_addrlen_,
        recv_buf_, sizeof(recv_buf_), sock->priv->socket->priv, xsink);
}

QoreHashNode* SocketQuicSendStreamingResponsePollOperation::continuePoll(ExceptionSink* xsink) {
    // Reassign the input stream to the current (worker) thread on first call
    if (need_reassign) {
        need_reassign = false;
        if (input_stream) {
            input_stream->reassignThread(xsink);
            if (*xsink) {
                return nullptr;
            }
        }
    }

    AutoLocker al(sock->priv->m);

    if (!sock->priv->socket->priv->isOpen()) {
        xsink->raiseException("QUIC-POLL-ERROR", "socket closed during poll operation");
        return nullptr;
    }

    // Guard against continuePoll() after abort()
    if (!quic_session) {
        xsink->raiseException("QUIC-POLL-ERROR", "QUIC streaming send session has been aborted");
        return nullptr;
    }

    auto cancel_stream = [&]() {
        sock->priv->m.unlock();
        ExceptionSink cancel_xsink;
        sock->cancelQuicStreamAsync(session_id, stream_id, &cancel_xsink);
        cancel_xsink.clear();
        sock->priv->m.lock();
    };

    while (true) {
        switch (ss_state) {
            case QCS_SS::SUBMIT_HEADERS: {
                // Submit the streaming response (headers only, deferred data reader)
                int rv = quic_session->submitResponseStreaming(stream_id, status_code, header_map, xsink);
                if (rv && !*xsink) {
                    xsink->raiseException("QUIC-ERROR", "failed to submit HTTP/3 streaming response: rv=%d", rv);
                }
                if (*xsink) {
                    return nullptr;
                }

                printd(5, "SocketQuicSendStreamingResponsePollOperation() headers submitted stream_id=%" PRId64
                    "\n", stream_id);
                ss_state = QCS_SS::READ_CHUNK;
                continue;
            }

            case QCS_SS::READ_CHUNK: {
                if (eof) {
                    // Send end-of-stream marker
                    int rv = quic_session->sendStreamData(stream_id, nullptr, 0, true, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (rv == 1) {
                        // Buffer full, wait for ACKs
                        ss_state = QCS_SS::RECV_ACK;
                        continue;
                    }
                    ss_state = QCS_SS::FLUSH;
                    continue;
                }

                if (is_pollable) {
                    // Non-blocking read for pollable streams
                    assert(stream_fd >= 0);
                    struct pollfd pfd;
                    pfd.fd = stream_fd;
                    pfd.events = POLLIN;
                    pfd.revents = 0;
                    int poll_rv = poll(&pfd, 1, 0);
                    if (poll_rv < 0) {
                        xsink->raiseException("QUIC-ERROR", "poll() on stream fd failed: %s",
                            strerror(errno));
                        cancel_stream();
                        return nullptr;
                    }
                    if (poll_rv == 0) {
                        // Stream not ready: register fd with event loop and
                        // retry reading on next continuePoll() call.
                        std::vector<std::pair<int, int>> extra_fds{{stream_fd, SOCK_POLLIN}};
                        QoreHashNode* poll_info = getSocketPollInfoHash(xsink, SOCK_POLLIN, extra_fds);
                        if (poll_info) {
                            setPollTimeoutFromExpiry(poll_info, quic_session->getExpiry(), xsink);
                        }
                        return poll_info;
                    }

                    // Stream FD is readable — do non-blocking read
                    SimpleRefHolder<BinaryNode> chunk(new BinaryNode);
                    chunk->preallocate(chunk_size);
                    int64 count = input_stream->readNonBlock(
                        const_cast<void*>(chunk->getPtr()), chunk_size, xsink);
                    if (*xsink) {
                        cancel_stream();
                        return nullptr;
                    }
                    if (count == 0) {
                        // poll said readable but read returned 0 → EOF
                        eof = true;
                        continue;
                    }
                    chunk->setSize(count);
                    current_chunk = chunk.release();
                } else {
                    // Non-pollable streams: release the socket lock before
                    // reading to avoid blocking the I/O thread if the
                    // InputStream implementation performs blocking I/O
                    sock->priv->m.unlock();
                    current_chunk = input_stream->readHelper(chunk_size, xsink);
                    sock->priv->m.lock();
                    // Re-validate session after re-acquiring lock (abort() may
                    // have been called while the lock was released)
                    if (!quic_session) {
                        if (!*xsink) {
                            xsink->raiseException("QUIC-POLL-ERROR",
                                "QUIC streaming send session was aborted during InputStream read");
                        }
                        return nullptr;
                    }
                    if (*xsink) {
                        cancel_stream();
                        return nullptr;
                    }
                    if (!current_chunk) {
                        eof = true;
                        continue;
                    }
                }

                printd(5, "SocketQuicSendStreamingResponsePollOperation::continuePoll() "
                    "read chunk size=%zu\n", current_chunk->size());
                ss_state = QCS_SS::SEND_CHUNK;
                continue;
            }

            case QCS_SS::SEND_CHUNK: {
                int rv = quic_session->sendStreamData(stream_id, current_chunk->getPtr(),
                    current_chunk->size(), false, xsink);
                if (*xsink) {
                    return nullptr;
                }
                if (rv == 1) {
                    // Buffer full (backpressure) — wait for ACKs before retrying
                    ss_state = QCS_SS::RECV_ACK;
                    continue;
                }
                current_chunk = nullptr;
                ss_state = QCS_SS::FLUSH;
                continue;
            }

            case QCS_SS::FLUSH: {
                // Check for peer disconnect
                if (quic_session->isClosed()) {
                    ss_state = QCS_SS::DONE;
                    if (set_non_block) {
                        sock->priv->clearNonBlock();
                        set_non_block = false;
                    }
                    return nullptr;
                }

                // Read any incoming ACKs to update flow control windows
                recvAndProcessPackets(xsink);
                if (*xsink) {
                    return nullptr;
                }

                // Coalesced: timer check + packet generation + next expiry
                ngtcp2_tstamp next_expiry;
                int rv = sendPendingPackets(next_expiry, xsink);
                if (*xsink) {
                    return nullptr;
                }
                if (rv == SOCK_POLLOUT) {
                    QoreHashNode* poll_info = getSocketPollInfoHash(xsink, SOCK_POLLIN | SOCK_POLLOUT);
                    if (poll_info) {
                        setPollTimeoutFromExpiry(poll_info, next_expiry, xsink);
                    }
                    return poll_info;
                }

                // Check if there's still more data to send
                if (quic_session->hasPendingWrite()) {
                    QoreHashNode* poll_info = getSocketPollInfoHash(xsink, SOCK_POLLIN);
                    if (poll_info) {
                        // flow-control-blocked pending data: don't sub-ms tight-poll
                        // on the PTO timer; wait for the unblocking ACK / MAX_STREAM_DATA
                        setPollTimeoutFromExpiry(poll_info, next_expiry, xsink, true);
                    }
                    return poll_info;
                }

                if (eof) {
                    // All done
                    ss_state = QCS_SS::DONE;
                    if (set_non_block) {
                        sock->priv->clearNonBlock();
                        set_non_block = false;
                    }
                    return nullptr;
                }

                // More data to read
                ss_state = QCS_SS::READ_CHUNK;
                continue;
            }

            case QCS_SS::RECV_ACK: {
                // Read incoming ACKs
                recvAndProcessPackets(xsink);
                if (*xsink) {
                    return nullptr;
                }

                // Coalesced: timer check + packet generation + send
                ngtcp2_tstamp next_expiry;
                {
                    int rv = sendPendingPackets(next_expiry, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                }

                // Retry the previous send/EOF operation
                if (eof && !current_chunk) {
                    // Was trying to send EOF marker
                    int rv = quic_session->sendStreamData(stream_id, nullptr, 0, true, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (rv != 1) {
                        // Buffer drained; proceed to flush
                        ss_state = QCS_SS::FLUSH;
                        continue;
                    }
                    // Still full — fall through to yield below
                } else if (current_chunk) {
                    // Retry sending the chunk
                    int rv = quic_session->sendStreamData(stream_id,
                        current_chunk->getPtr(), current_chunk->size(), false, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (rv != 1) {
                        // Buffer drained; proceed to flush
                        current_chunk = nullptr;
                        ss_state = QCS_SS::FLUSH;
                        continue;
                    }
                    // Still full — fall through to yield below
                } else {
                    // No pending send — go back to read
                    ss_state = QCS_SS::READ_CHUNK;
                    continue;
                }

                // Buffer still full — yield to event loop and wait for ACKs
                {
                    QoreHashNode* poll_info = getSocketPollInfoHash(xsink, SOCK_POLLIN);
                    if (poll_info) {
                        setPollTimeoutFromExpiry(poll_info, next_expiry, xsink);
                    }
                    return poll_info;
                }
            }

            case QCS_SS::DONE:
                if (set_non_block) {
                    sock->priv->clearNonBlock();
                    set_non_block = false;
                }
                return nullptr;

            default:
                xsink->raiseException("QUIC-POLL-ERROR",
                    "invalid streaming send response poll state: %d",
                    static_cast<int>(ss_state));
                return nullptr;
        }
    }
}

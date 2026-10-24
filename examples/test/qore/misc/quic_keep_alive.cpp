/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

// The keep-alive interval of a QUIC connection with active streams: half of its effective idle timeout, so both
// endpoints' idle timers are refreshed, but at most 15 s, so a NAT that forgets a UDP flow after 30 s of silence
// keeps it (QuicKeepAlive.h)

#include "qore/intern/QuicKeepAlive.h"

#include <cstdint>
#include <cstdio>
#include <stdexcept>

static unsigned cases = 0;

static constexpr uint64_t NS_PER_SECOND = 1000000000ULL;

static void check(uint64_t effective_idle_ns, uint64_t expected_ns, const char* message) {
    if (qore_quic_active_stream_keep_alive_ns(effective_idle_ns) != expected_ns) {
        throw std::runtime_error(message);
    }
    ++cases;
}

// the cap is a compile-time property of the policy
static_assert(QORE_QUIC_ACTIVE_STREAM_KEEP_ALIVE_NS == 15 * NS_PER_SECOND, "the cap is 15 s");
static_assert(qore_quic_active_stream_keep_alive_ns(75 * NS_PER_SECOND) == 15 * NS_PER_SECOND,
    "the default 75 s idle timeout is capped");
static_assert(qore_quic_active_stream_keep_alive_ns(30 * NS_PER_SECOND) == 15 * NS_PER_SECOND,
    "half of 30 s is the cap itself");

int main() {
    try {
        // Qore's own idle timeout (QuicSession::QUIC_IDLE_TIMEOUT_NS): half of it is 37.5 s, after a 30 s NAT
        // forgot the flow
        check(75 * NS_PER_SECOND, 15 * NS_PER_SECOND, "75 s idle: PING every 15 s, not 37.5 s");
        // a peer advertising a longer idle timeout than ours (the effective value is the lesser)
        check(180 * NS_PER_SECOND, 15 * NS_PER_SECOND, "180 s idle: capped");
        check(UINT64_MAX - 1, 15 * NS_PER_SECOND, "the largest finite idle timeout: capped");
        // at and around the cap
        check(30 * NS_PER_SECOND, 15 * NS_PER_SECOND, "30 s idle: half is the cap");
        check(30 * NS_PER_SECOND + 2, 15 * NS_PER_SECOND, "just above 30 s: capped");
        check(30 * NS_PER_SECOND - 2, 15 * NS_PER_SECOND - 1, "just below 30 s: half");
        // a short idle timeout keeps PINGing at half of it, so both idle timers are still refreshed
        check(10 * NS_PER_SECOND, 5 * NS_PER_SECOND, "10 s idle: PING every 5 s");
        check(3 * NS_PER_SECOND + 1, 3 * NS_PER_SECOND / 2, "odd nanoseconds round down");
        // a degenerate idle timeout still gives an interval of at least 1 ns, never 0
        check(2, 1, "2 ns idle: 1 ns");
        check(1, 1, "1 ns idle: at least 1 ns");
        check(0, 1, "0 idle: at least 1 ns");
    } catch (const std::exception& e) {
        fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
    printf("PASS: %u QUIC keep-alive interval cases\n", cases);
    return 0;
}

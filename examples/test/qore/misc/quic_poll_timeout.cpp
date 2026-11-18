/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

// The poll timeout of a QUIC operation honours every timer of its connection, however close or late
// (QuicPollTimeout.h).  The cases model the slow paths that postponed a delayed acknowledgement by a second, which
// then put the next keep-alive PING at the idle deadline: the earliest timer is ngtcp2's pacing timer some
// microseconds after a packet was sent, or a timer that came due while the I/O thread was descheduled between
// running the connection's timers and computing its poll timeout.

#include "qore/intern/QuicPollTimeout.h"

#include <cstdint>
#include <cstdio>
#include <stdexcept>

static unsigned cases = 0;

static constexpr uint64_t NS_PER_US = 1000ULL;
static constexpr uint64_t NS_PER_MS = 1000000ULL;
static constexpr uint64_t NS_PER_SECOND = 1000000000ULL;

// an arbitrary point on the monotonic clock of ngtcp2, far from both ends of its range
static constexpr uint64_t NOW = 873366915886266ULL;

static void check(uint64_t expiry, uint64_t now, int64_t expected_ms, const char* message) {
    int64_t ms = qore_quic_poll_timeout_ms(expiry, now);
    if (ms != expected_ms) {
        char buf[512];
        snprintf(buf, sizeof buf, "%s: expected %lld ms, got %lld ms", message, (long long)expected_ms,
            (long long)ms);
        throw std::runtime_error(buf);
    }
    ++cases;
}

// the policy is a compile-time function
static_assert(QORE_QUIC_NO_TIMER_POLL_MS == 1000, "the timeout with no timer pending is 1 s");
static_assert(qore_quic_poll_timeout_ms(UINT64_MAX, NOW) == QORE_QUIC_NO_TIMER_POLL_MS, "no timer: the fallback");
static_assert(qore_quic_poll_timeout_ms(NOW + 52 * NS_PER_US, NOW) == 1, "a pacing timer 52 us ahead: 1 ms");
static_assert(qore_quic_poll_timeout_ms(NOW - 1, NOW) == 1, "a timer just due: 1 ms");

int main() {
    try {
        // no timer pending: the fallback
        check(UINT64_MAX, NOW, QORE_QUIC_NO_TIMER_POLL_MS, "no timer pending");
        check(UINT64_MAX, 0, QORE_QUIC_NO_TIMER_POLL_MS, "no timer pending at the start of the clock");

        // ngtcp2's pacing timer, due some microseconds after each packet sent, is often the earliest timer; the
        // delayed acknowledgement (25 ms) and the keep-alive PING come after it.  Waiting for the fallback instead
        // postponed them all by a second (an acknowledgement sent with ack_delay=1001 ms)
        check(NOW + 52 * NS_PER_US, NOW, 1, "pacing timer 52 us ahead");
        check(NOW + 277 * NS_PER_US, NOW, 1, "pacing timer 277 us ahead");
        check(NOW + 1, NOW, 1, "a timer 1 ns ahead");
        check(NOW + NS_PER_MS, NOW, 1, "a timer exactly 1 ms ahead");

        // the I/O thread is descheduled between running the timers and computing the timeout, past the next timer:
        // the timer has not been run, and the next pass runs it at once instead of a second later
        check(NOW, NOW, 1, "a timer due now");
        check(NOW - 1, NOW, 1, "a timer due 1 ns ago");
        check(NOW - 24 * NS_PER_MS, NOW, 1, "a delayed acknowledgement due 24 ms ago");
        check(NOW - 5 * NS_PER_SECOND, NOW, 1, "a keep-alive due 5 s ago (a long stall)");
        check(0, NOW, 1, "the earliest possible timer");

        // timers ahead are rounded up, so the operation never wakes before one is due
        check(NOW + NS_PER_MS + 1, NOW, 2, "1 ms + 1 ns ahead");
        check(NOW + 24 * NS_PER_MS + 600 * NS_PER_US, NOW, 25, "a delayed acknowledgement 24.6 ms ahead");
        check(NOW + 25 * NS_PER_MS, NOW, 25, "exactly 25 ms ahead");
        check(NOW + 999 * NS_PER_MS + 1, NOW, 1000, "a keep-alive PING just under 1 s ahead");

        // timers further ahead than the fallback are honoured too: the fallback is no cap
        check(NOW + NS_PER_SECOND, NOW, 1000, "1 s ahead");
        check(NOW + 5 * NS_PER_SECOND, NOW, 5000, "a 5 s keep-alive");
        check(NOW + 75 * NS_PER_SECOND, NOW, 75000, "a 75 s idle timeout");

        // the largest finite expiry does not overflow
        check(UINT64_MAX - 1, 0, static_cast<int64_t>((UINT64_MAX - 1) / NS_PER_MS + 1),
            "the largest finite expiry from the start of the clock");
    } catch (const std::exception& e) {
        fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
    printf("PASS: %u QUIC poll timeout cases\n", cases);
    return 0;
}

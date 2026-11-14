/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QuicPollTimeout.h

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

#ifndef _QORE_INTERN_QUICPOLLTIMEOUT_H
#define _QORE_INTERN_QUICPOLLTIMEOUT_H

#include <cstdint>

//! The poll timeout of a QUIC operation when its connection has no timer pending, in milliseconds
static constexpr int64_t QORE_QUIC_NO_TIMER_POLL_MS = 1000;

//! Returns the poll timeout of a QUIC operation, in milliseconds, for the next timer of its connection
/** The timeout ends when the connection's earliest timer (ngtcp2_conn_get_expiry2()) is due, so that the operation
    runs it on time.  That timer is often ngtcp2's pacing timer, due some microseconds after each packet sent, with
    the timers that matter after it: the delayed acknowledgement (max_ack_delay), the probe timeout, the keep-alive
    PING.  Every timer is therefore honoured, however close:

    - a timer due within a millisecond waits 1 ms, never longer: waiting for a fallback instead postpones every later
      timer with it
    - a timer already due waits 1 ms: it came due after the operation ran the connection's timers (the thread was
      descheduled between running them and computing this timeout), so the next pass of the operation runs it

    Waiting QORE_QUIC_NO_TIMER_POLL_MS instead in these cases delayed an acknowledgement by a second; ngtcp2 counts
    the keep-alive interval from the last packet sent or received, so the late acknowledgement put the next PING at
    the idle deadline, and the connection was closed by its idle timer (NGTCP2_ERR_IDLE_CLOSE) although both ends
    were alive.  No pass loops on a timer that is due: running the timers clears each one that is due or sets it
    again in the future.

    @param expiry the earliest timer of the connection (ngtcp2 timestamp, ns), or UINT64_MAX for none
    @param now the current time (ngtcp2 timestamp, ns)

    @return the poll timeout in milliseconds: at least 1, or QORE_QUIC_NO_TIMER_POLL_MS if no timer is pending
*/
static constexpr int64_t qore_quic_poll_timeout_ms(uint64_t expiry, uint64_t now) {
    if (expiry == UINT64_MAX) {
        return QORE_QUIC_NO_TIMER_POLL_MS;
    }
    if (expiry <= now) {
        return 1;
    }
    const uint64_t timeout_ns = expiry - now;
    // rounded up, so that the operation never wakes before the timer is due
    const uint64_t timeout_ms = timeout_ns / 1000000 + (timeout_ns % 1000000 ? 1 : 0);
    return timeout_ms > static_cast<uint64_t>(INT64_MAX) ? INT64_MAX : static_cast<int64_t>(timeout_ms);
}

#endif

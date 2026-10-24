/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QuicKeepAlive.h

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

#ifndef _QORE_INTERN_QUICKEEPALIVE_H
#define _QORE_INTERN_QUICKEEPALIVE_H

#include <cstdint>

//! The longest interval between the keep-alive PINGs of a QUIC connection with active streams, in nanoseconds
/** A middlebox forgets a UDP flow after much less silence than a QUIC idle timeout (RFC 9000 §10.1.2): Linux
    conntrack keeps a flow whose packets all fell within its first two seconds for nf_conntrack_udp_timeout (30 s
    by default), and NATs commonly time out UDP at 30 s.  A response sent after that silence is dropped until the
    client sends again, so with PINGs every half of a 75 s idle timeout a request answered after 30 s waited for
    the PING at 37.5 s, or its deadline expired first.  15 s is the interval of Chromium's QUIC client while streams
    are open.
*/
static constexpr uint64_t QORE_QUIC_ACTIVE_STREAM_KEEP_ALIVE_NS = 15ULL * 1000000000ULL;

//! Returns the keep-alive interval of a QUIC connection with active streams, in nanoseconds
/** @param effective_idle_ns the connection's effective idle timeout (RFC 9000 §10.1.2: the lesser of the values
    both endpoints advertise), in nanoseconds

    @return half of @p effective_idle_ns, so that both endpoints' idle timers are refreshed, but at most
    @ref QORE_QUIC_ACTIVE_STREAM_KEEP_ALIVE_NS, so that the path's UDP flow state is refreshed too; at least 1
*/
static constexpr uint64_t qore_quic_active_stream_keep_alive_ns(uint64_t effective_idle_ns) {
    const uint64_t half = effective_idle_ns > 1 ? effective_idle_ns / 2 : 1;
    return half < QORE_QUIC_ACTIVE_STREAM_KEEP_ALIVE_NS ? half : QORE_QUIC_ACTIVE_STREAM_KEEP_ALIVE_NS;
}

#endif

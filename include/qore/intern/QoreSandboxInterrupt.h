/* Copyright (c) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */

#ifndef _QORE_SANDBOX_INTERRUPT_H
#define _QORE_SANDBOX_INTERRUPT_H

#include <atomic>
#include <cstdint>

// Conservative count of interrupted managers, including requests being published.
// Zero lets cancellation points skip manager resolution, but never thread cancellation.
// Updates and reads must remain seq_cst: this is part of the wait/broadcast handshake.
DLLLOCAL extern std::atomic<uint64_t> qore_pending_program_interrupts;

#endif

/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  SendRateWindow.h

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

#ifndef _QORE_INTERN_SENDRATEWINDOW_H

#define _QORE_INTERN_SENDRATEWINDOW_H

#include <array>
#include <cstddef>
#include <cstdint>

//! The average rate at which a peer takes data while a sender waits for it, over a sliding window (qore#7280)
/** Only time that the sender spends waiting for the peer counts, and only the bytes that the peer took in that time:
    a connection with nothing to send is never below the rate, and data that the transport accepted without a wait
    is no credit for a later wait.  The window is kept across sends, so that a peer cannot reset it with each message.

    The state is a fixed ring of samples, so an update takes constant time and memory.  The class does no locking.
*/
class SendRateWindow {
public:
    //! The number of samples that the window is divided into
    static constexpr size_t Samples = 16;

    //! The progress step assumed for a peer before one has been observed, in bytes
    static constexpr int64_t MinProgressStep = 128 * 1024;

    //! The largest progress step that is allowed for, in bytes
    static constexpr int64_t MaxProgressStep = 8 * 1024 * 1024;

    //! The most time without any delivery that a step can be allowed, in milliseconds, unless the window is longer
    static constexpr int64_t MaxStepGraceMs = 10 * 60 * 1000;

    //! Deliveries within this time are one step, in milliseconds
    static constexpr int64_t StepGapMs = 1000;

    //! The most credit that can be held
    static constexpr int64_t CreditLimit = INT64_MAX / 2;

    //! Creates the window
    /** @param min_rate the minimum average rate in bytes per second; 0 disables the check
        @param window_ms the length of the window in milliseconds, which must be positive if \a min_rate is
    */
    SendRateWindow(int64_t min_rate = 0, int64_t window_ms = 0) : min_rate(min_rate), window_ms(window_ms) {
    }

    //! Returns True if the check is enabled
    bool enabled() const {
        return min_rate > 0 && window_ms > 0;
    }

    int64_t getMinRate() const {
        return min_rate;
    }

    int64_t getWindowMs() const {
        return window_ms;
    }

    //! Returns the longest time that a waiting sender should wait before it updates the window, in milliseconds
    int64_t getCheckIntervalMs() const {
        int64_t rv = window_ms / static_cast<int64_t>(Samples);
        return rv > 0 ? rv : 1;
    }

    //! Makes the check allow for a transport that shows the peer's progress only in steps
    /** A TCP receiver opens its window in large steps, so that the sender of a peer reading steadily at a low rate
        sees no progress for a long time and then a whole step.  With this set, the check is a credit: each byte that
        the peer takes buys the time that it takes at the minimum rate, and time spent waiting uses it up.  The credit
        holds at most one step at the minimum rate (the largest step seen, from @ref MinProgressStep to
        @ref MaxProgressStep, for at most @ref MaxStepGraceMs), and at least one window.  A peer that stays below the
        minimum rate therefore always runs out of credit, however its deliveries are timed.
    */
    void enableProgressSteps() {
        progress_steps = true;
    }

    //! Sets the most waiting time that one update of the credit can charge, in milliseconds; 0 means no limit
    /** A sender that waits wakes at least every @ref getCheckIntervalMs(), so a longer time between two updates is a
        delay of the sender itself, such as a stopped or overloaded process, and is not charged to the peer.
    */
    void setMaxWaitCharge(int64_t ms) {
        max_wait_charge_ms = ms;
    }

    //! Returns the most waiting time that one update of the credit can charge, in milliseconds; 0 means no limit
    int64_t getMaxWaitCharge() const {
        return max_wait_charge_ms;
    }

    //! Returns true if the check allows for progress in steps
    bool hasProgressSteps() const {
        return progress_steps;
    }

    //! Returns the time that the credit lasts without any delivery in milliseconds, or 0 if not applicable
    int64_t getStepGraceMs() const {
        if (!progress_steps || !enabled()) {
            return 0;
        }
        const int64_t capacity = getCreditCapacity();
        return capacity / min_rate + (capacity % min_rate ? 1 : 0);
    }

    //! Records the state of the send; returns true if the peer is below the minimum rate
    /** @param total_bytes the bytes that the peer has taken so far on the connection or stream; this never
        decreases; a negative value means that the count is not known, which is no progress
        @param waiting true if the sender is waiting for the peer from now on
        @param now_ms the current time on a monotonic clock in milliseconds

        @return true if the peer took less than the minimum rate over a whole window of waiting
    */
    bool update(int64_t total_bytes, bool waiting, int64_t now_ms) {
        if (!enabled()) {
            return false;
        }
        if (progress_steps) {
            return updateCredit(total_bytes, waiting, now_ms);
        }
        if (have_last) {
            if (last_waiting) {
                if (now_ms > last_ms) {
                    waited_ms += now_ms - last_ms;
                }
                if (total_bytes > last_bytes) {
                    taken += total_bytes - last_bytes;
                }
            }
        }
        if (total_bytes > last_bytes || !have_last) {
            last_bytes = total_bytes < 0 ? (have_last ? last_bytes : 0) : total_bytes;
        }
        last_ms = now_ms;
        last_waiting = waiting;
        have_last = true;

        // a new sample each time the waiting time has passed a slot of the window
        if (!count || waited_ms - at(count - 1).waited_ms >= getCheckIntervalMs()) {
            push({waited_ms, taken});
        }
        // the oldest sample is the newest one that is at least a whole window old
        while (count > 1 && at(1).waited_ms <= waited_ms - window_ms) {
            head = (head + 1) % ring.size();
            --count;
        }
        const Sample& base = at(0);
        const int64_t span = waited_ms - base.waited_ms;
        if (span < window_ms) {
            return false;
        }
        // a required amount that cannot be represented is more than any peer can have taken
        const int64_t whole_seconds = span / 1000 + 1;
        const int64_t required = min_rate > INT64_MAX / whole_seconds
            ? INT64_MAX
            // in this order so that large values do not overflow: whole seconds, then the remainder
            : min_rate * (span / 1000) + min_rate / 1000 * (span % 1000) + min_rate % 1000 * (span % 1000) / 1000;
        return taken - base.taken < required;
    }

private:
    struct Sample {
        int64_t waited_ms;
        int64_t taken;
    };

    // one more than the slots of the window, so that the oldest sample can be a whole window old
    std::array<Sample, Samples + 2> ring = {};
    size_t head = 0;
    size_t count = 0;

    int64_t min_rate;
    int64_t window_ms;
    //! the time spent waiting so far
    int64_t waited_ms = 0;
    //! the bytes taken while waiting so far
    int64_t taken = 0;
    int64_t last_ms = 0;
    int64_t last_bytes = 0;
    bool last_waiting = false;
    bool have_last = false;

    //! true if the check allows for progress in steps
    bool progress_steps = false;
    //! the most waiting time that one update of the credit can charge; 0 means no limit
    int64_t max_wait_charge_ms = 0;
    //! true once the credit has been set
    bool have_credit = false;
    //! the credit, in bytes times 1000: a delivered byte adds 1000, a millisecond of waiting takes the minimum rate
    int64_t credit = 0;
    //! the largest step seen
    int64_t largest_step = 0;
    //! the bytes of the current step
    int64_t current_step = 0;
    //! the time when the current step started
    int64_t step_start_ms = 0;

    //! Returns the most credit that can be held
    int64_t getCreditCapacity() const {
        const int64_t step = largest_step > MinProgressStep ? largest_step : MinProgressStep;
        // no overflow: the step is at most MaxProgressStep
        int64_t capacity = step * 1000;
        if (capacity / min_rate >= MaxStepGraceMs) {
            // no overflow: the minimum rate is less than the step in bytes times 1000 over the time
            capacity = MaxStepGraceMs * min_rate;
        }
        // the credit lasts at least a window, as the check without steps does
        const int64_t window_credit = min_rate > CreditLimit / window_ms ? CreditLimit : min_rate * window_ms;
        return window_credit > capacity ? window_credit : capacity;
    }

    bool updateCredit(int64_t total_bytes, bool waiting, int64_t now_ms) {
        if (!have_credit) {
            credit = getCreditCapacity();
            have_credit = true;
        }
        const bool was_waiting = have_last && last_waiting;
        const int64_t delta = have_last && total_bytes > last_bytes ? total_bytes - last_bytes : 0;
        int64_t waited = was_waiting && now_ms > last_ms ? now_ms - last_ms : 0;
        if (max_wait_charge_ms > 0 && waited > max_wait_charge_ms) {
            waited = max_wait_charge_ms;
        }
        if (total_bytes > last_bytes || !have_last) {
            last_bytes = total_bytes < 0 ? (have_last ? last_bytes : 0) : total_bytes;
        }
        last_ms = now_ms;
        last_waiting = waiting;
        have_last = true;
        waited_ms += waited;

        // the time is charged first, so that a step that arrives at the end of a wait pays for that wait; the limit
        // keeps the sums below from overflowing, with the credit at most CreditLimit
        credit -= waited > CreditLimit / min_rate ? CreditLimit : waited * min_rate;
        if (delta > 0) {
            const int64_t old_capacity = getCreditCapacity();
            // what the peer took within a short time, waiting or not, shows how much its buffers can hold
            if (now_ms - step_start_ms >= StepGapMs) {
                step_start_ms = now_ms;
                current_step = 0;
            }
            current_step = current_step > MaxProgressStep - delta || delta > MaxProgressStep
                ? MaxProgressStep : current_step + delta;
            if (current_step > largest_step) {
                largest_step = current_step;
            }
            const int64_t capacity = getCreditCapacity();
            if (was_waiting) {
                credit += delta > CreditLimit / 1000 ? CreditLimit : delta * 1000;
            } else {
                // data taken without a wait pays for no wait, but a larger step needs its larger allowance
                credit += capacity - old_capacity;
            }
            if (credit > capacity) {
                credit = capacity;
            }
        }
        if (credit > 0) {
            return false;
        }
        credit = 0;
        // never before a whole window of waiting, also at a rate whose credit cannot be represented
        return waited_ms >= window_ms;
    }

    const Sample& at(size_t i) const {
        return ring[(head + i) % ring.size()];
    }

    void push(const Sample& s) {
        if (count == ring.size()) {
            head = (head + 1) % ring.size();
            --count;
        }
        ring[(head + count) % ring.size()] = s;
        ++count;
    }
};

#endif // _QORE_INTERN_SENDRATEWINDOW_H

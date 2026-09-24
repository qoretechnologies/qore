/* -*- indent-tabs-mode: nil -*- */
/*
  RSection.cpp

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

#include <qore/Qore.h>
#include <qore/intern/RSection.h>

// does not block if the rsection cannot be acquired, returns -1 if the lock cannot be acquired and sets a notification
int qore_rsection_priv::tryRSectionLockNotifyWaitRead(RNotifier* rn) {
    assert(has_notify);

    int tid = q_gettid();

    AutoLocker al(l);

    // if we already have the rsection, then return
    if (rs_tid == tid) {
        return 0;
    }

    // The write lock is strictly stronger than the rsection: it excludes every reader, every writer and
    // every other rsection holder, so a thread holding it already has the exclusive access the rsection
    // grants, and checkRSectionExclusive() treats the two as equivalent.  Take the rsection directly
    // instead of registering a notification: the only thread that could ever release this write lock is
    // the caller, which is about to roll the scan back and wait, so a notification here is a wait for
    // ourselves that nothing can end.  This is reachable whenever code holding an object's write lock
    // runs Qore code that dereferences an object in the same recursive set - a destructor run from an
    // lvalue operation, for example.  The read count is balanced by the matching rSectionUnlock() and
    // cannot admit another thread while the write lock is held.
    if (write_tid == tid) {
        ++readers;
        rs_tid = tid;
        return 0;
    }

    while (true) {
        // If another thread owns the write lock or the rsection, abort this scan and retry after that owner
        // releases it.  RSet scanning calls this while holding RSet read locks; blocking for one of those
        // owners can deadlock with a writer that needs to invalidate the same RSet.
        if (write_tid != -1 || rs_tid != -1) {
            setNotificationIntern(rn);
            return -1;
        }

        if (!rs_shared.load(std::memory_order_relaxed)) {
            break;
        }

        // Shared holders are scans, and a scan never blocks: it releases every lock it holds before waiting,
        // and it never takes an RSet write lock, so waiting for the shared holders to leave always ends.
        // Waiting is also what keeps a scan that has to change a recursive set from being starved: while
        // rsection_waiting is set, tryRSectionLockSharedNotifyWaitRead() admits no new scan.
        ++rsection_waiting;
        rsection_cond.wait(l);
        --rsection_waiting;
    }

    // grab the read lock
    ++readers;

    // grab the rsection
    rs_tid = tid;
    return 0;
}

// does not block if the rsection cannot be taken in shared mode, returns -1 and sets a notification
int qore_rsection_priv::tryRSectionLockSharedNotifyWaitRead(RNotifier* rn, bool& shared) {
    assert(has_notify);

    int tid = q_gettid();
    shared = false;

    AutoLocker al(l);

    // an rsection that this thread already holds is not taken again, and the caller does not release it
    if (rs_tid == tid) {
        return 0;
    }

    // a write lock held by this thread is stronger than the rsection; see
    // tryRSectionLockNotifyWaitRead() for why the rsection is granted directly rather than waited for
    if (write_tid == tid) {
        ++readers;
        rs_tid = tid;
        return 0;
    }

    // A shared holder excludes a writer and an exclusive rsection holder, so a thread already waiting for
    // the rsection must not be overtaken by an unbounded stream of scans: a dereference deciding whether a
    // recursive set can be collected waits there, and starving it would stop collection altogether.
    if (write_tid != -1 || rs_tid != -1 || rsection_waiting) {
        setNotificationIntern(rn);
        return -1;
    }

    // grab the read lock and the rsection in shared mode
    ++readers;
    rs_shared.fetch_add(1, std::memory_order_relaxed);
    shared = true;
    return 0;
}

/*
    VRMutex.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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
#include "qore/intern/VRMutex.h"

#include <cassert>

VRMutex::VRMutex() {
}

int VRMutex::enter(ExceptionSink *xsink) {
    int mtid = q_gettid();
    VLock *nvl = getVLock();
    AutoLocker al(&asl_lock);
    int rc = VRMutex::grabImpl(mtid, nvl, xsink);
    if (!rc)
        mark_and_push(mtid, nvl);
    return rc;
}

int VRMutex::exit() {
    AutoLocker al(&asl_lock);
    int rc = VRMutex::releaseImpl();
    if (!rc)
        release_and_signal();
    return rc;
}

int VRMutex::cleanupImpl() {
    if (tid == q_gettid()) {
        release_and_signal();
        count = 0;
        return -1;
    }
    return 0;
}

int VRMutex::grabImpl(int mtid, VLock *nvl, ExceptionSink *xsink, int64 timeout_ms) {
    if (tid != mtid) {
        // a wait that ends with the lock still held waits again only for the rest of the timeout
        QoreWaitDeadline deadline(timeout_ms);
        while (tid != Lock_Unlocked) {
            if (tid == Lock_Deleted) {
                xsink->raiseException("LOCK-ERROR", "TID %d cannot execute %s::enter() because the object has been " \
                    "deleted in another thread", mtid, getName());
                return -1;
            }

            int64 wait_ms = timeout_ms;
            if (deadline.active() && !(wait_ms = deadline.remainingMs())) {
                return -1;
            }
            ++waiting;
            int rc = nvl->waitOn((AbstractSmartLock *)this, vl, xsink, wait_ms);
            --waiting;
            // if rc is non-zero there was a timeout or deadlock
            if (rc) {
                return -1;
            }
        }
        // the thread lock list must always be the same if the lock was grabbed
        assert((mtid == tid  && vl == nvl) || (tid == Lock_Unlocked && !vl));
    }
    printd(5, "VRMutex::enter() this=%p count: %d->%d\n", this, count, count + 1);

    return count++;
}

int VRMutex::tryGrabImpl(int mtid, VLock *nvl) {
    if (tid != mtid) {
        if (tid != Lock_Unlocked) {
            return -1;
        }

        // the thread lock list must always be the same if the lock was grabbed
        assert((mtid == tid  && vl == nvl) || (tid == Lock_Unlocked && !vl));
    }

    printd(5, "VRMutex::enter() this=%p count: %d->%d\n", this, count, count + 1);

    return count++;
}

// internal use only
int VRMutex::releaseImpl() {
    assert(tid == q_gettid());
    printd(5, "VRMutex::exit() this=%p count: %d->%d\n", this, count, count - 1);

    --count;
    // if this is the last thread from the group to exit the lock, then return 0
    return count ? -1 : 0;
}

int VRMutex::releaseImpl(ExceptionSink* xsink) {
    int mtid = q_gettid();
    if (tid == Lock_Unlocked) {
        // use getName() here so it can be safely inherited
        xsink->raiseException("LOCK-ERROR", "TID %d called %s::exit() without acquiring the lock", mtid, getName());
        return -1;
    }
    if (tid == Lock_Deleted) {
        xsink->raiseException("LOCK-ERROR", "TID %d cannot execute %s::exit() because the object has been deleted " \
            "in another thread", mtid, getName());
        return -1;
    }
    if (tid != mtid) {
        // use getName() here so it can be safely inherited
        xsink->raiseException("LOCK-ERROR", "TID %d called %s::exit() while the lock is held by TID %d", mtid,
            getName(), tid);
        return -1;
    }
    // count must be > 0 because tid > 0
    assert(count);

    printd(5, "VRMutex::exit() this=%p count: %d->%d\n", this, count, count - 1);

    --count;
    // if this is the last thread from the group to exit the lock, then return 0
    return count ? -1 : 0;
}

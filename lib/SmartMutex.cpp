/*
  SmartMutex.cpp

  Qore Programming Language

  Copyright (C) 2003 - 2026 David Nichols

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
#include <qore/SmartMutex.h>

#include <cassert>

SmartMutex::~SmartMutex() {
    assert(cmap.empty());
}

int SmartMutex::releaseImpl() {
    if (tid < 0) {
        return -1;
    }
    return 0;
}

int SmartMutex::grabImpl(int mtid, VLock *nvl, ExceptionSink *xsink, int64 timeout_ms) {
    if (tid == mtid) {
        // getName() for possible inheritance
        xsink->raiseException("LOCK-ERROR", "TID %d called %s::lock() twice without an intervening %s::unlock()", tid, getName(), getName());
        return -1;
    }
    while (tid >= 0) {
        waiting++;
        int rc =  nvl->waitOn((AbstractSmartLock *)this, vl, xsink, timeout_ms);
        waiting--;
        if (rc) {
            return -1;
        }
    }
    if (tid == Lock_Deleted) {
        // getName() for possible inheritance
        xsink->raiseException("LOCK-ERROR", "%s has been deleted in another thread", getName());
        return -1;
    }
    return 0;
}

int SmartMutex::releaseImpl(ExceptionSink *xsink) {
    int mtid = q_gettid();
    if (tid < 0) {
        // getName() for possible inheritance
        xsink->raiseException("LOCK-ERROR", "TID %d called %s::unlock() while the lock was already unlocked", mtid, getName());
        return -1;
    }
    if (tid != mtid) {
        // getName() for possible inheritance
        xsink->raiseException("LOCK-ERROR", "TID %d called %s::unlock() while the lock is held by tid %d", mtid, getName(), tid);
        return -1;
    }
    return 0;
}

int SmartMutex::tryGrabImpl(int mtid, VLock *nvl) {
    if (tid != Lock_Unlocked) {
        return -1;
    }
    return 0;
}

int SmartMutex::externWaitImpl(int mtid, QoreCondition *cond, ExceptionSink *xsink, int64 timeout_ms) {
    // make sure this TID owns the lock
    if (verify_wait_unlocked(mtid, xsink)) {
        return -1;
    }

    // insert into cond map
    cond_map_t::iterator i = cmap.find(cond);
    if (i == cmap.end()) {
        i = cmap.insert(std::make_pair(cond, 1)).first;
    } else {
        ++(i->second);
    }

    // save vlock
    VLock *nvl = vl;

    // release lock
    release_intern();

    // wait for condition
    int rc = timeout_ms > 0 ? cond->wait2(&asl_lock, timeout_ms) : cond->wait(&asl_lock);

    // decrement cond count and delete from map if 0
    if (!--(i->second)) {
        cmap.erase(i);
    }

    // reacquire the lock
    if (grabImpl(mtid, nvl, xsink)) {
        return -1;
    }

    grab_intern(mtid, nvl);
    return rc;
}

void SmartMutex::destructorImpl(ExceptionSink *xsink) {
    cond_map_t::iterator i = cmap.begin(), e = cmap.end();
    if (i != e) {
        xsink->raiseException("LOCK-ERROR", "%s object deleted in TID %d while one or more Condition variables were "
            "waiting on it", getName(), q_gettid());
        // wake up all condition variables waiting on this mutex
        for (; i != e; i++) {
            i->first->broadcast();
        }
    }
}

bool SmartMutex::owns_lock() {
    AutoLocker al(&asl_lock);
    return tid == q_gettid() ? true : false;
}

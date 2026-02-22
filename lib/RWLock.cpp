/*
    RWLock.cpp

    Read-Write Lock object (default: prefer readers to allow recursively grabbing the read lock)
    prefer writers not yet tested/completely implemented (ex: should throw an exception if a
    thread holding the read lock tries to recursively grab the read lock)

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
#include "qore/RWLock.h"

#include <cassert>

RWLock::RWLock(bool p) : readRequests(0), prefer_writers(p), num_readers(0) {
}

RWLock::~RWLock() {
    assert(tmap.empty());
    assert(cmap.empty());
}

int RWLock::numReaders() {
    return num_readers;
}

int RWLock::externWaitImpl(int mtid, QoreCondition *cond, ExceptionSink *xsink, int64 timeout_ms) {
    // make sure this TID owns the lock
    if (mtid == tid) { // in write lock
        // insert into cond map
        cond_map_t::iterator i = cmap.find(cond);
        if (i == cmap.end()) {
            i = cmap.insert(std::make_pair(cond, 1)).first;
        } else {
            ++(i->second);
        }

        // save vlock
        VLock *nvl = vl;

        // record signal generation before releasing the lock so we can detect
        // signals fired at any point after this
        uint64_t gen = cond->getSignalGen();

        // release lock
        release_intern();

        int rc;
        // check if a signal/broadcast was already fired before we start waiting
        if (cond->getSignalGen() != gen) {
            rc = 0;
        } else {
            // wait for condition
            rc = timeout_ms > 0 ? cond->wait2(&asl_lock, timeout_ms) : cond->wait(&asl_lock);
        }

        // detect signals/broadcasts that were fired while this thread was waiting
        // (before we erase from the condition map); save result before erasing so we
        // don't access cond after it's no longer tracked (another thread might delete it)
        bool signal_fired = (rc == ETIMEDOUT && cond->getSignalGen() != gen);

        // decrement cond count and delete from map if 0
        if (!--(i->second)) {
            cmap.erase(i);
        }

        // reacquire the lock
        if (grabImpl(mtid, nvl, xsink)) {
            return -1;
        }

        grab_intern(mtid, nvl);

        // if a signal/broadcast was fired during the gap when we weren't listening
        // (while reacquiring the Qore-level mutex in grabImpl), treat as success
        if (signal_fired) {
            rc = 0;
        }

        return rc;
    }

    if (tid == Lock_Deleted) {
        xsink->raiseException("LOCK-ERROR", "TID %d trying to wait on %s object, but it has been deleted in " \
            "another thread", mtid, getName());
        return -1;
    }

    tid_map_t::iterator i = tmap.find(mtid);
    if (i == tmap.end()) {
        xsink->raiseException("LOCK-ERROR", "TID %d trying to wait on %s object while not holding either the read " \
            "or write lock", mtid, getName());
        return -1;
    }
    // issue #2817: handle the case when the read lock is held recursively
    int read_count = i->second;

    // in read lock
    // insert into cond map
    cond_map_t::iterator ci = cmap.find(cond);
    if (ci == cmap.end()) {
        ci = cmap.insert(std::make_pair(cond, 1)).first;
    } else {
        ++(ci->second);
    }

    // save vlock
    VLock *nvl = vmap[mtid];

    // record signal generation before releasing the lock so we can detect
    // signals fired at any point after this
    uint64_t gen = cond->getSignalGen();

    // release lock
    // issue #2817: handle the case when the read lock is held recursively
    for (int j = 0; j < read_count; ++j) {
        release_read_lock_intern(i);
    }

    int rc;
    // check if a signal/broadcast was already fired before we start waiting
    if (cond->getSignalGen() != gen) {
        rc = 0;
    } else {
        // wait for condition
        rc = timeout_ms ? cond->wait(&asl_lock, timeout_ms) : cond->wait(&asl_lock);
    }

    // detect signals/broadcasts that were fired while this thread was waiting
    // (before we erase from the condition map); save result before erasing so we
    // don't access cond after it's no longer tracked (another thread might delete it)
    bool signal_fired = (rc == ETIMEDOUT && cond->getSignalGen() != gen);

    // decrement cond count and delete from map if 0
    if (!--(ci->second)) {
        cmap.erase(ci);
    }

    // reacquire the lock
    // issue #2817: handle the case when the read lock is held recursively
    for (int j = 0; j < read_count; ++j) {
        if (grab_read_lock_intern(mtid, nvl, 0, xsink)) {
            return -1;
        }
    }

    // if a signal/broadcast was fired during the gap when we weren't listening
    // (while reacquiring the read lock), treat as success
    if (signal_fired) {
        rc = 0;
    }

    return rc;
}

int RWLock::grabImpl(int mtid, VLock *nvl, ExceptionSink *xsink, int64 timeout_ms) {
    // check for errors
    if (tid == mtid) {
        xsink->raiseException("LOCK-ERROR", "TID %d tried to grab the write lock twice", tid);
        return -1;
    }
    // check if this thread holds a read lock - upgrading from read to write lock would deadlock
    if (tmap.find(mtid) != tmap.end()) {
        xsink->raiseException("LOCK-ERROR", "TID %d tried to acquire the write lock while holding the read lock; "
            "read-to-write lock upgrades are not supported and would result in a deadlock", mtid);
        return -1;
    }
    while (tid >= 0 || (tid == Lock_Unlocked && num_readers)) {
        ++waiting;
        int rc;
        // if the write lock is grabbed, send vl (only one thread owns the lock)
        if (tid >= 0)
            rc = nvl->waitOn((AbstractSmartLock *)this, vl, xsink, timeout_ms);
        else  // otherwise the read lock is grabbed, so send vmap (many threads own the lock)
            rc = nvl->waitOn((AbstractSmartLock *)this, vmap, xsink, timeout_ms);
        --waiting;
        if (rc)
            return -1;
    }
    if (tid == Lock_Deleted) {
        xsink->raiseException("LOCK-ERROR", "The %s object has been deleted in another thread", getName());
        return -1;
    }
    return 0;
}

void RWLock::signalAllImpl() {
    if (waiting)
        asl_cond.broadcast();
    if (readRequests)
        read.broadcast();
}

void RWLock::signalImpl() {
    if (prefer_writers) {
        if (waiting)
            asl_cond.signal();
        else if (readRequests)
            read.broadcast();
    } else {
        if (readRequests)
            read.broadcast();
        else if (waiting)
            asl_cond.signal();
    }
}

void RWLock::destructorImpl(ExceptionSink *xsink) {
    cond_map_t::iterator i = cmap.begin(), e = cmap.end();
    if (i != e) {
        xsink->raiseException("RWLOCK-ERROR", "%s object deleted in TID %d while one or more Condition variables " \
            "were waiting on it", getName(), q_gettid());
        // wake up all condition variables waiting on this mutex
        for (; i != e; i++)
            i->first->broadcast();
    }

    if (num_readers)
        asl_cond.broadcast();

    num_readers = 0;

    // release all read locks, but do not remove from the thread resource list
    for (vlock_map_t::iterator vi = vmap.begin(), ve = vmap.end(); vi != ve; ++vi)
        vi->second->pop((AbstractSmartLock*)this);

    vmap.clear();
    tmap.clear();
}

// return value 0 = last read lock for this thread released
int RWLock::cleanup_read_lock_intern(tid_map_t::iterator i) {
    if (!--(i->second)) {
        vlock_map_t::iterator vi = vmap.find(i->first);
        // pop the lock from this thread's lock list
        vi->second->pop((AbstractSmartLock *)this);
        // do not remove from the thread resource list here
        // remove map entries for this thread
        tmap.erase(i);
        vmap.erase(vi);
        assert((!tmap.empty() && num_readers) || (tmap.empty() && !num_readers));
        return 0;
    }
    return -1;
}

// internal use only - releases read and write locks
int RWLock::releaseImpl() {
    if (num_readers) {
        // signal writers if any are waiting
        if (!--num_readers && waiting)
            asl_cond.signal();

        int mtid = q_gettid();
        // remove reader for this thread
        tid_map_t::iterator ti = tmap.find(mtid);
        assert(ti != tmap.end());

        cleanup_read_lock_intern(ti);
        return -1;
    } else if (tid >= 0) {
        return 0;
    }
    return -1;
}

// thread exited holding the lock: remove whatever lock was locked
int RWLock::cleanupImpl() {
    int rc = 0;
    // if it was a read lock
    if (num_readers) {
        int mtid = q_gettid();
        // remove reader for this thread
        vlock_map_t::iterator vi = vmap.find(mtid);

        // vmap entry must be present for this thread
        assert(vi != vmap.end());

        // delete entry from the thread lock list
        vi->second->pop(this);
        // erase vlock map entry
        vmap.erase(vi);
        tid_map_t::iterator ti = tmap.find(mtid);

        // tmap entry must be present if we found a vmap entry
        assert(ti != tmap.end());

        // subtract number of times this thread held the read lock from num_readers
        num_readers -= ti->second;
        // wake up threads waiting on write lock, if any
        if (!num_readers && waiting)
            asl_cond.signal();

        // erase thread map entry
        tmap.erase(ti);
        assert((!tmap.empty() && num_readers) || (tmap.empty() && !num_readers));
        rc = -1;
    } else if (tid >= 0) { // if it was the write lock
        // this thread must own the lock
        assert(tid == q_gettid());
        // mark lock as unlocked
        tid = -1;

        // delete entry from the thread lock list
        vl->pop(this);

        vl = 0;
        // wake up sleeping thread(s)
        signalImpl();

        rc = -1;
    }

    return rc;
}

int RWLock::releaseImpl(ExceptionSink *xsink) {
    int mtid = q_gettid();
    if (tid == Lock_Deleted) {
        xsink->raiseException("LOCK-ERROR", "The %s object has been deleted in another thread", getName());
        return -1;
    }
    if (tid == Lock_Unlocked) {
        xsink->raiseException("LOCK-ERROR", "TID %d called %s::writeUnlock() while not holding the write lock", mtid,
            getName());
        return -1;
    }
    if (tid != mtid) {
        // use getName() here so it can be safely inherited
        xsink->raiseException("LOCK-ERROR", "%s::writeUnlock() called by TID %d while the write lock is held by " \
            "TID %d", getName(), mtid, tid);
        return -1;
    }
    return 0;
}

int RWLock::tryGrabImpl(int mtid, class VLock *nvl) {
    if (tid != Lock_Unlocked || num_readers)
        return -1;

    return 0;
}

void RWLock::set_initial_read_lock_intern(int mtid, VLock *nvl) {
    // only set these values the first time the lock is acquired
    tmap[mtid] = 1;
    vmap[mtid] = nvl;
    // now register that we have grabbed this lock with the thread list
    nvl->push((AbstractSmartLock *)this);
    // register the thread resource
    set_thread_resource((AbstractThreadResource *)this);
}

void RWLock::mark_read_lock_intern(int mtid, VLock *nvl) {
    ++num_readers;

    // add read lock to thread and vlock maps
    // (do not set vl, set in vmap instead)
    tid_map_t::iterator i = tmap.find(mtid);
    if (i == tmap.end())
        set_initial_read_lock_intern(mtid, nvl);
    else // increment lock count otherwise
        ++(i->second);
}

int RWLock::readLock(ExceptionSink *xsink, int64 timeout_ms) {
    int mtid = q_gettid();
    VLock *nvl = getVLock();
    SafeLocker sl(&asl_lock);

    if (tid == mtid) {
        xsink->raiseException("LOCK-ERROR", "TID %d called %s::readLock() while holding the write lock", mtid,
            getName());
        return -1;
    }

    return grab_read_lock_intern(mtid, nvl, timeout_ms, xsink);
}

// assumes the write lock is not grabbed by this thread
int RWLock::grab_read_lock_intern(int mtid, VLock *nvl, int64 timeout_ms, ExceptionSink *xsink) {
    if (tid >= 0) {
        do {
            ++readRequests;
            int rc;
            rc = nvl->waitOn((AbstractSmartLock *)this, &read, vl, xsink, timeout_ms);
            --readRequests;
            if (rc)
                return -1;
        } while (tid >= 0);

        if (tid == Lock_Deleted) {
            xsink->raiseException("LOCK-ERROR", "The %s object has been deleted in another thread", getName());
            return -1;
        }
        ++num_readers;

        // we know that this thread was not in the read lock because the write lock was grabbed before
        set_initial_read_lock_intern(mtid, nvl);
        return 0;
    }

    if (tid == Lock_Deleted) {
        xsink->raiseException("LOCK-ERROR", "The %s object has been deleted in another thread", getName());
        return -1;
    }

    // read lock must be open
    mark_read_lock_intern(mtid, nvl);

    return 0;
}

void RWLock::release_read_lock_intern(tid_map_t::iterator i) {
    // signal writers if any are waiting
    if (!--num_readers && waiting)
        asl_cond.signal();

    // if this thread has released it's last read lock, then remove the thread resource
    if (!cleanup_read_lock_intern(i))
        remove_thread_resource((AbstractThreadResource *)this);
}

int RWLock::readUnlock(ExceptionSink* xsink) {
    int mtid = q_gettid();
    AutoLocker al(&asl_lock);
    if (tid == mtid) {
        xsink->raiseException("LOCK-ERROR", "TID %d called %s::readUnlock() while holding the write lock", mtid,
            getName());
        return -1;
    }

    if (tid == Lock_Deleted) {
        xsink->raiseException("LOCK-ERROR", "The %s object has been deleted in another thread", getName());
        return -1;
    }

    tid_map_t::iterator i = tmap.find(mtid);
    if (i == tmap.end()) {
        xsink->raiseException("LOCK-ERROR", "TID %d called %s::readUnlock() while not holding the read lock", mtid,
            getName());
        return -1;
    }

    release_read_lock_intern(i);
    return 0;
}

int RWLock::tryReadLock() {
    AutoLocker al(&asl_lock);
    if (tid != Lock_Unlocked)
        return -1;

    mark_read_lock_intern(q_gettid(), getVLock());

    return 0;
}

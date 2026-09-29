/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    RWLock.h

    Read-Write Lock object (default: prefer readers)

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

#ifndef _QORE_CLASS_RWLOCK

#define _QORE_CLASS_RWLOCK

#include "qore/AbstractSmartLock.h"

#include <map>

// to track TIDs and read counts of readers
typedef std::map<int, int> tid_map_t;

// ASL mapping:
// asl_cond: write cond
// waiting: waiting write requests
// tid: write TID

// forward references
class VLock;
typedef std::map<int, VLock*> vlock_map_t;

class RWLock : public AbstractSmartLock {
public:
    DLLEXPORT RWLock(bool p = false);

    DLLEXPORT virtual ~RWLock();

    DLLEXPORT int readLock(ExceptionSink *xsink, int64 timeout_ms = 0);
    DLLEXPORT int readUnlock(ExceptionSink *xsink);
    DLLEXPORT int tryReadLock();

    DLLEXPORT int numReaders();

    DLLLOCAL int getReadWaiting() const {
        return readRequests;
    }
    DLLLOCAL int getWriteWaiting() const {
        return waiting;
    }

    //! Returns true if the calling thread holds the lock for reading or writing
    /** The lock state is read under the lock's internal mutex: the owner and reader count are changed by other
        threads, so reading them without it is a data race even though the caller only asks about itself.
    */
    DLLLOCAL bool lockOwner() const {
        int mtid = q_gettid();
        AutoLocker al(&asl_lock);
        return tid == mtid || readLockOwnerIntern(mtid);
    }

    //! Returns true if the calling thread holds the write lock
    DLLLOCAL bool writeLockOwner() const {
        int mtid = q_gettid();
        AutoLocker al(&asl_lock);
        return tid == mtid;
    }

    //! Returns true if the calling thread holds the read lock
    DLLLOCAL bool readLockOwner() const {
        int mtid = q_gettid();
        AutoLocker al(&asl_lock);
        return readLockOwnerIntern(mtid);
    }

    DLLLOCAL virtual const char* getName() const {
        return "RWLock";
    }

private:
    int readRequests;
    QoreCondition read;
    bool prefer_writers;
    tid_map_t tmap;     // map of TIDs to read lock counts
    vlock_map_t vmap;   // map of TIDs to VLock data structures
    int num_readers;    // number of threads holding the read lock

    //! Returns true if \a mtid holds the read lock; called with asl_lock held
    DLLLOCAL bool readLockOwnerIntern(int mtid) const {
        // if the write lock is held or the lock is deleted or nobody has the read lock, then return false
        if (tid > -1 || tid == Lock_Deleted || !num_readers) {
            return false;
        }
        return tmap.find(mtid) != tmap.end();
    }

    // 0 = last read lock in this thread released
    DLLLOCAL int cleanup_read_lock_intern(tid_map_t::iterator i);
    DLLLOCAL void mark_read_lock_intern(int mtid, VLock *nvl);
    DLLLOCAL void release_read_lock_intern(tid_map_t::iterator i);
    DLLLOCAL int grab_read_lock_intern(int mtid, VLock *nvl, int64 timeout_ms, ExceptionSink *xsink);
    DLLLOCAL void set_initial_read_lock_intern(int mtid, VLock *nvl);

    DLLLOCAL virtual int cleanupImpl();
    DLLLOCAL virtual void signalAllImpl();
    DLLLOCAL virtual void signalImpl();
    DLLLOCAL virtual int releaseImpl();
    DLLLOCAL virtual int releaseImpl(ExceptionSink *xsink);
    DLLLOCAL virtual int grabImpl(int mtid, VLock *nvl, ExceptionSink *xsink, int64 timeout_ms = 0);
    DLLLOCAL virtual int tryGrabImpl(int mtid, VLock *nvl);
    DLLLOCAL virtual int externWaitImpl(int mtid, QoreCondition *cond, ExceptionSink *xsink, int64 timeout_ms = 0);
    DLLLOCAL virtual void destructorImpl(ExceptionSink *xsink);
};

#endif // _QORE_CLASS_RWLOCK

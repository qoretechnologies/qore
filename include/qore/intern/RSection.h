/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  RSection.h

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

#ifndef _QORE_INTERN_RSECTION_H

#define _QORE_INTERN_RSECTION_H

#include <atomic>

#include "qore/intern/qore_var_rwlock_priv.h"

// forward references
class qore_rsection_priv;

class RNotifier {
public:
    bool setp = false;
    //! the thread holding the lock this notification waits for, or -1 when only shared holders or a waiting
    //! exclusive acquisition conflicted; written under the lock's own mutex when the notification is registered
    int owner_tid = -1;
    //! the scan holding the rsection that could not be taken, or 0 when the conflicting owner is not a scan
    /** When it is set, no notification is registered: the thread waits for that scan to complete instead of for
        the rsection to be released, which happens at every rollback of the scan as well; see
        RSetHelper::RSetHelper() and design/dgc.md, "A scan waits for the scan it collided with to complete".
        Written under the lock's own mutex.
    */
    uint64_t owner_scan = 0;
    QoreThreadLock m;
    QoreCondition c;

    DLLLOCAL RNotifier() {
    }

    DLLLOCAL ~RNotifier() {
        assert(!setp);
    }

    DLLLOCAL void done() {
        AutoLocker al(m);
        assert(setp);
        setp = false;
        c.signal();
    }

    DLLLOCAL void set() {
        AutoLocker al(m);
        assert(!setp);
        setp = true;
    }

    DLLLOCAL void wait() {
        AutoLocker al(m);
        while (setp) {
            c.wait(m);
        }
    }

private:
    RNotifier(const RNotifier&) = delete;
    RNotifier& operator=(const RNotifier&) = delete;
 };

typedef std::list<RNotifier*> n_list_t;

// rwlock with standard read and write lock handling and special "rsection" handling
// the rsection is grabbed with the read lock but only one thread can have the rsection lock at once
// leaving other threads to read the object normally
class qore_rsection_priv : public qore_var_rwlock_priv {
public:
    DLLLOCAL qore_rsection_priv() {
        has_notify = true;
    }

    DLLLOCAL virtual ~qore_rsection_priv() {
        assert(rs_tid == -1);
        assert(list.empty());
    }

    // returns -1 and sets a notification if the rsection or the write lock is held by another thread; waits
    // only for scans holding the rsection in shared mode, which cannot block in turn
    /** @param scan the scan taking the rsection (RSetHelper's sequence number), or 0 for a thread that cannot wait
        for a scan to complete; recorded while it holds the rsection, so that a scan that cannot take it waits for
        that scan to complete (RNotifier::owner_scan), and only a scan with a number waits for one to complete
    */
    DLLLOCAL int tryRSectionLockNotifyWaitRead(RNotifier* rn, uint64_t scan = 0);

    //! Takes the rsection in shared mode, which excludes writers and exclusive holders but not other scans
    /** Scans read the graph; only a scan that has to change a recursive set needs the rsection to itself, and
        RSetHelper starts that scan over in exclusive mode.  Does not block: returns -1 and registers a
        notification, or records the scan holding the rsection, when the rsection cannot be taken.  See
        design/dgc.md.

        @param scan the scan taking the rsection, or 0; a scan with a number waits for a scan holding the rsection
        exclusively to complete, as in tryRSectionLockNotifyWaitRead()
    */
    DLLLOCAL int tryRSectionLockSharedNotifyWaitRead(RNotifier* rn, bool& shared, uint64_t scan = 0);

    DLLLOCAL void upgradeReadToRSection(int tid = q_gettid()) {
        AutoLocker al(l);
        assert(write_tid == -1);

        while (rs_tid != -1 || rs_shared.load(std::memory_order_relaxed)) {
            ++rsection_waiting;
            rsection_cond.wait(l);
            --rsection_waiting;
        }

        rs_tid  = tid;
    }

    DLLLOCAL void rSectionUnlock() {
        AutoLocker al(l);
        // the rsection can be held on top of this thread's own write lock; see
        // tryRSectionLockNotifyWaitRead()
        assert(write_tid == -1 || write_tid == q_gettid());
        assert(rs_tid == q_gettid());
        assert(readers);

        // unlock rsection
        rs_tid = -1;
        rs_scan = 0;

        // Threads that registered a notification while this thread also held the write lock cannot
        // acquire the rsection until that lock is released, and qore_var_rwlock_priv::unlock() notifies
        // them then; waking them here would only make them restart their scans and register again.
        if (write_tid == -1 && !rs_shared.load(std::memory_order_relaxed)) {
            qore_rsection_priv::notifyIntern();
        }

        // a thread waiting for the rsection can only take it once no shared holder is left
        if (rsection_waiting && !rs_shared.load(std::memory_order_relaxed))
            rsection_cond.signal();

        if (!--readers)
            unlock_read_signal();
    }

    //! Releases a shared rsection taken with tryRSectionLockSharedNotifyWaitRead()
    DLLLOCAL void rSectionUnlockShared() {
        AutoLocker al(l);
        assert(write_tid == -1);
        assert(rs_tid == -1);
        assert(rs_shared.load(std::memory_order_relaxed) > 0);
        assert(readers);

        if (rs_shared.fetch_sub(1, std::memory_order_relaxed) == 1) {
            // the rsection is free now, so scans and threads waiting for it can take it
            qore_rsection_priv::notifyIntern();
            if (rsection_waiting) {
                rsection_cond.signal();
            }
        }

        if (!--readers)
            unlock_read_signal();
    }

    DLLLOCAL bool hasRSectionLock(int tid = q_gettid()) {
        return rs_tid == tid;
    }

    DLLLOCAL bool checkRSectionExclusive(int tid = q_gettid()) {
        return (rs_tid == tid || write_tid == tid);
    }

    //! Returns true if the rsection is held, in either mode; for assertions only
    DLLLOCAL bool checkRSectionHeld(int tid = q_gettid()) {
        return checkRSectionExclusive(tid) || rs_shared.load(std::memory_order_relaxed) > 0;
    }

    DLLLOCAL int rSectionTid() const {
        return rs_tid;
    }

protected:
    // tid of thread holding the rsection lock
    // atomic because it is read without lock in hasRSectionLock() and checkRSectionExclusive()
    std::atomic_int rs_tid{-1};

    //! the scan that took the rsection exclusively, or 0 if it is not held or is held by anything else
    /** Anything else is a dereference deciding a verdict, or a thread that held the rsection or the write lock before
        its scan started; a scan that collides with one of those waits for the rsection to be released, as that holder
        can be waiting for the colliding scan's thread.  Written and read under l.
    */
    uint64_t rs_scan = 0;

    // number of threads waiting on the rsection lock
    int rsection_waiting = 0;

    // the number of scans holding the rsection in shared mode; they exclude writers and exclusive holders
    // but not each other
    // written under l; atomic because checkRSectionHeld() reads it without l
    std::atomic_int rs_shared{0};

    // rsection condition variablt
    QoreCondition rsection_cond;

    // list of ObjectRSetHelper objects for notifications for rsection management
    n_list_t list;

    // notify rsection threads that the rsection lock has been released
    DLLLOCAL virtual void notifyIntern() {
        for (n_list_t::iterator i = list.begin(), e = list.end(); i != e; ++i) {
            (*i)->done();
        }
        list.clear();
    }

    DLLLOCAL void setNotificationIntern(RNotifier* rn) {
        assert(write_tid != -1 || rs_tid != -1 || rs_shared.load(std::memory_order_relaxed) || rsection_waiting);
        list.push_back(rn);
        rn->owner_tid = write_tid != -1 ? write_tid : rs_tid;
        rn->owner_scan = 0;
        rn->set();
        //printd(5, "qrp::sNI t: %p r: %p\n", this, rn);
    }

    //! Records a conflict with another scan, or registers a notification for any other conflict
    /** A scan that holds the rsection exclusively is waited for until it completes rather than until it releases the
        rsection, which it does at every rollback too: waking on a rollback restarts the waiting scan beside the scan
        it collided with, and scans of one graph from different roots that keep doing that never complete.  See
        design/dgc.md, "A scan waits for the scan it collided with to complete".

        @param rn the notification of the scan that could not take the rsection
        @param scan that scan's number, or 0 if its thread cannot wait for a scan to complete, as it has no scan
        slot; it then waits for the release
    */
    DLLLOCAL void setConflictIntern(RNotifier* rn, uint64_t scan) {
        if (scan && write_tid == -1 && rs_tid != -1 && rs_scan) {
            rn->owner_tid = rs_tid;
            rn->owner_scan = rs_scan;
            return;
        }
        setNotificationIntern(rn);
    }

private:
    qore_rsection_priv(const qore_rsection_priv&) = delete;
    qore_rsection_priv& operator=(const qore_rsection_priv&) = delete;
};

class RSectionLock : public QoreVarRWLock {
public:
    DLLLOCAL RSectionLock() : QoreVarRWLock(new qore_rsection_priv) {
    }

    DLLLOCAL ~RSectionLock() {
    }

    // does not block under any circumstances, returns -1 if the lock cannot be acquired and sets a notification
    DLLLOCAL int tryRSectionLockNotifyWaitRead(RNotifier* rn, uint64_t scan = 0) {
        assert(priv->write_tid >= -1);
        return static_cast<qore_rsection_priv*>(priv)->tryRSectionLockNotifyWaitRead(rn, scan);
    }

    DLLLOCAL int tryRSectionLockSharedNotifyWaitRead(RNotifier* rn, bool& shared, uint64_t scan = 0) {
        assert(priv->write_tid >= -1);
        return static_cast<qore_rsection_priv*>(priv)->tryRSectionLockSharedNotifyWaitRead(rn, shared, scan);
    }

    DLLLOCAL void rSectionUnlock() {
        assert(priv->write_tid >= -1);
        static_cast<qore_rsection_priv*>(priv)->rSectionUnlock();
    }

    DLLLOCAL void rSectionUnlockShared() {
        assert(priv->write_tid >= -1);
        static_cast<qore_rsection_priv*>(priv)->rSectionUnlockShared();
    }

    DLLLOCAL bool hasRSectionLock(int tid = q_gettid()) {
        assert(priv->write_tid >= -1);
        return static_cast<qore_rsection_priv*>(priv)->hasRSectionLock(tid);
    }

    DLLLOCAL bool checkRSectionExclusive(int tid = q_gettid()) {
        assert(priv->write_tid >= -1);
        return static_cast<qore_rsection_priv*>(priv)->checkRSectionExclusive(tid);
    }

    DLLLOCAL bool checkRSectionHeld(int tid = q_gettid()) {
        assert(priv->write_tid >= -1);
        return static_cast<qore_rsection_priv*>(priv)->checkRSectionHeld(tid);
    }

    DLLLOCAL void upgradeReadToRSection(int tid = q_gettid()) {
        assert(priv->write_tid >= -1);
        static_cast<qore_rsection_priv*>(priv)->upgradeReadToRSection(tid);
    }

    DLLLOCAL int rSectionTid() const {
        assert(priv->write_tid >= -1);
        return static_cast<qore_rsection_priv*>(priv)->rSectionTid();
    }
};

class QoreSafeRSectionReadLocker : private QoreSafeVarRWReadLocker {
public:
    DLLLOCAL QoreSafeRSectionReadLocker(RSectionLock& n_l) : QoreSafeVarRWReadLocker(n_l), has_rsection(false) {
    }

    DLLLOCAL ~QoreSafeRSectionReadLocker() {
        if (locked && has_rsection) {
            static_cast<RSectionLock*>(l)->rSectionUnlock();
            locked = false;
        }
    }

    DLLLOCAL void acquireRSection(int tid = q_gettid()) {
        static_cast<RSectionLock*>(l)->upgradeReadToRSection(tid);
        has_rsection = true;
    }

    //! unlocks the object and updates the locked flag, assumes that the lock is held
    DLLLOCAL void unlock() {
        assert(locked);
        locked = false;

        if (has_rsection) {
            static_cast<RSectionLock*>(l)->rSectionUnlock();
        } else {
            l->unlock();
        }
    }

private:
    bool has_rsection;
};

class QoreRSectionLocker : private QoreSafeVarRWReadLocker {
public:
    DLLLOCAL QoreRSectionLocker(RSectionLock& n_l) : QoreSafeVarRWReadLocker(n_l) {
        static_cast<RSectionLock*>(l)->upgradeReadToRSection();
    }

    DLLLOCAL ~QoreRSectionLocker() {
        static_cast<RSectionLock*>(l)->rSectionUnlock();
        locked = false;
    }
};

#endif

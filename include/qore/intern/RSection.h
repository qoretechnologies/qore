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

    // does not block if there is an rsection conflict, returns -1 if the lock cannot be acquired and sets a
    // notification
    DLLLOCAL int tryRSectionLockNotifyWaitRead(RNotifier* rn);

    DLLLOCAL void upgradeReadToRSection(int tid = q_gettid()) {
        AutoLocker al(l);
        assert(write_tid == -1);

        while (rs_tid != -1) {
            ++rsection_waiting;
            rsection_cond.wait(l);
            --rsection_waiting;
        }

        rs_tid  = tid;
    }

    DLLLOCAL void rSectionUnlock() {
        AutoLocker al(l);
        assert(write_tid == -1);
        assert(rs_tid == q_gettid());
        assert(readers);

        // unlock rsection
        rs_tid = -1;

        qore_rsection_priv::notifyIntern();

        if (rsection_waiting)
            rsection_cond.signal();

        if (!--readers)
            unlock_read_signal();
    }

    DLLLOCAL bool hasRSectionLock(int tid = q_gettid()) {
        return rs_tid == tid;
    }

    DLLLOCAL bool checkRSectionExclusive(int tid = q_gettid()) {
        return (rs_tid == tid || write_tid == tid);
    }

    DLLLOCAL int rSectionTid() const {
        return rs_tid;
    }

protected:
    // tid of thread holding the rsection lock
    // atomic because it is read without lock in hasRSectionLock() and checkRSectionExclusive()
    std::atomic_int rs_tid{-1};

    // number of threads waiting on the rsection lock
    int rsection_waiting = 0;

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
        assert(write_tid != -1 || rs_tid != -1);
        list.push_back(rn);
        rn->set();
        //printd(5, "qrp::sNI t: %p r: %p\n", this, rn);
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
    DLLLOCAL int tryRSectionLockNotifyWaitRead(RNotifier* rn) {
        assert(priv->write_tid >= -1);
        return static_cast<qore_rsection_priv*>(priv)->tryRSectionLockNotifyWaitRead(rn);
    }

    DLLLOCAL void rSectionUnlock() {
        assert(priv->write_tid >= -1);
        static_cast<qore_rsection_priv*>(priv)->rSectionUnlock();
    }

    DLLLOCAL bool hasRSectionLock(int tid = q_gettid()) {
        assert(priv->write_tid >= -1);
        return static_cast<qore_rsection_priv*>(priv)->hasRSectionLock(tid);
    }

    DLLLOCAL bool checkRSectionExclusive(int tid = q_gettid()) {
        assert(priv->write_tid >= -1);
        return static_cast<qore_rsection_priv*>(priv)->checkRSectionExclusive(tid);
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

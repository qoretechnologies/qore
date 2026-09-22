/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_var_rwlock_priv.h

    internal read-write lock for variables

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

#ifndef _QORE_VAR_RWLOCK_PRIV_H
#define _QORE_VAR_RWLOCK_PRIV_H

#include <atomic>

// forward reference
//class QoreVarRWLock;

//! Read-write lock with a read path that takes no lock at all
/** This lock guards every object's members, every global and closure-bound variable and the public QoreRWLock and
    QoreVarRWLock, so every member or variable read takes and releases its read lock.  Readers only increment and
    decrement an atomic count; the mutex (l) is taken by writers, by the r-section, and by readers only when a writer
    holds, or is taking, the lock.

    The reader and a writer meet in a Dekker handshake: a reader increments readers and then loads write_claim; a
    writer, holding l, stores write_claim and then loads readers, all sequentially consistent.  So either the
    reader sees the writer and backs out to wait on l, or the writer sees the reader and waits for it to leave.
    A writer that finds readers drops its claim again before it releases l to wait, so readers keep their
    preference over waiting writers, which is what allows a thread holding the read lock to take it again (see
    lib/QoreRWLock.cpp); under l, write_claim is set exactly when write_tid is.
*/
class qore_var_rwlock_priv {
public:
    QoreThreadLock l;
    //! the TID of the thread holding the write lock, or -1; written under l
    /** Atomic because unlock() reads it without l to tell a write unlock from a read unlock: while it is set, no other
        thread holds the lock, since a writer only takes the lock once no reader is left.
    */
    std::atomic_int write_tid{-1};
    //! the number of read locks held, counting the one under an r-section; readers change it with no lock
    std::atomic_int readers{0};
    //! the number of threads waiting for a writer to release the lock; written and read under l
    int read_waiting = 0;
    //! the number of threads waiting for the write lock; incremented under l BEFORE the readers are checked
    /** Atomic because the last reader to leave reads it without l after its decrement, to know whether a writer has
        to be woken; one of them always sees the other.
    */
    std::atomic_int write_waiting{0};
    //! set while a writer holds the lock or is checking whether it can take it; written under l
    std::atomic_bool write_claim{false};
    QoreCondition write_cond,
        read_cond;
    bool has_notify = false;

    //! creates and initializes the lock
    DLLLOCAL qore_var_rwlock_priv() {
        //printd(5, "qore_var_rwlock_priv::qore_var_rwlock_priv() this: %p\n", this);
    }

    //! destroys the lock
    DLLLOCAL virtual ~qore_var_rwlock_priv() {
        //printd(5, "qore_var_rwlock_priv::~qore_var_rwlock_priv() this: %p\n", this);
    }

    //! grabs the write lock
    DLLLOCAL void wrlock() {
        int tid = q_gettid();
        assert(tid >= 0);
        AutoLocker al(l);
        assert(tid != write_tid);

        // announced before the readers are checked, so the last reader to leave cannot miss it; see unlockRead()
        ++write_waiting;
        while (!tryClaimWriteIntern(tid)) {
            write_cond.wait(l);
        }
        --write_waiting;
    }

    //! tries to grab the write lock; does not block if unsuccessful; returns 0 if successful
    DLLLOCAL int trywrlock() {
        int tid = q_gettid();
        assert(tid >= 0);
        AutoLocker al(l);
        assert(tid != write_tid);
        return tryClaimWriteIntern(tid) ? 0 : -1;
    }

    //! unlocks the lock (assumes the lock is locked)
    /** releases the write lock if one is held, otherwise releases a read lock
    */
    DLLLOCAL void unlock() {
        // A held write lock excludes all readers and all other writers, so it can only belong to
        // the calling thread; test for it directly instead of comparing the current TID to
        // write_tid.  q_gettid() returns 0 once the calling thread's data has been destroyed, and
        // locks are still grabbed while objects in the static namespace are destroyed, so the
        // comparison would fail for the write owner in exactly that window and take the read
        // branch instead: an abort on the assertion below in debug builds, and a corrupted reader
        // count that hangs every later writer in release builds.
        //
        // A read lock is released with no lock at all (unlockRead()); write_tid can be read without l here because
        // while it is set no other thread holds the lock.
        if (write_tid.load(std::memory_order_relaxed) == -1) {
            unlockRead();
            return;
        }

        AutoLocker al(l);
        assert(write_tid == -1 || !q_gettid() || write_tid == q_gettid());
        assert(write_tid != -1);
        write_tid.store(-1, std::memory_order_relaxed);
        // releases the lock to readers taking it with no lock: the store is sequentially consistent, so the data
        // written under the write lock is visible to a reader that sees the claim cleared
        write_claim.store(false, std::memory_order_seq_cst);
        if (has_notify) {
            notifyIntern();
        }

        unlock_signal();
    }

    //! grabs the read lock
    DLLLOCAL void rdlock() {
        if (tryReadFast()) {
            return;
        }

        AutoLocker al(l);
        assert(write_tid != q_gettid());
        while (write_tid != -1) {
            ++read_waiting;
            read_cond.wait(l);
            --read_waiting;
        }

        // no writer can claim the lock while l is held
        readers.fetch_add(1, std::memory_order_seq_cst);
    }

    //! tries to grab the read lock; does not block if unsuccessful; returns 0 if successful
    DLLLOCAL int tryrdlock() {
        if (tryReadFast()) {
            return 0;
        }

        AutoLocker al(l);
        assert(write_tid != q_gettid());
        if (write_tid != -1) {
            return -1;
        }

        readers.fetch_add(1, std::memory_order_seq_cst);
        return 0;
    }

    //! Releases a read lock with no lock at all, waking a waiting writer if this was the last reader
    DLLLOCAL void unlockRead() {
        assert(readers.load(std::memory_order_relaxed) > 0);
        // The writer's half of this handshake is in wrlock(): it increments write_waiting and then checks for
        // readers.  The decrement comes first here and write_waiting is loaded after it, both sequentially
        // consistent, so if the writer saw this reader, this reader sees the writer and wakes it.
        if (readers.fetch_sub(1, std::memory_order_seq_cst) == 1
            && write_waiting.load(std::memory_order_seq_cst)) {
            AutoLocker al(l);
            unlock_read_signal();
        }
    }

    DLLLOCAL void unlock_signal() {
        if (write_waiting) {
            write_cond.signal();
        } else if (read_waiting) {
            read_cond.broadcast();
        }
    }

    DLLLOCAL void unlock_read_signal() {
        //assert(!read_waiting);
        if (write_waiting) {
            write_cond.signal();
        }
    }

    DLLLOCAL static qore_var_rwlock_priv* get(QoreVarRWLock& l) {
        return l.priv;
    }

protected:
    //! Takes the read lock with no lock at all; returns false if a writer holds or is taking it
    DLLLOCAL bool tryReadFast() {
        // a writer holding the lock is seen before the count is touched
        if (write_claim.load(std::memory_order_acquire)) {
            return false;
        }
        // the reader's half of the handshake: increment, then check for a writer (see wrlock())
        readers.fetch_add(1, std::memory_order_seq_cst);
        if (!write_claim.load(std::memory_order_seq_cst)) {
            return true;
        }
        // a writer holds the lock or is checking whether it can take it: back out, and wake it if it saw this
        // reader and is now waiting for it
        unlockRead();
        return false;
    }

    //! Takes the write lock if no other thread holds it; called with l held
    /** Returns false if a writer holds it or readers are left, in which case the claim is dropped again: the
        caller waits on write_cond, and the last reader to leave wakes it.  Readers that saw the claim meanwhile
        backed out and wait for l, so they are not starved by a writer that could not take the lock.
    */
    DLLLOCAL bool tryClaimWriteIntern(int tid) {
        if (write_tid.load(std::memory_order_relaxed) != -1) {
            return false;
        }
        // the writer's half of the handshake: claim, then check for readers (see tryReadFast())
        write_claim.store(true, std::memory_order_seq_cst);
        if (!readers.load(std::memory_order_seq_cst)) {
            write_tid.store(tid, std::memory_order_relaxed);
            return true;
        }
        write_claim.store(false, std::memory_order_seq_cst);
        return false;
    }

    DLLLOCAL virtual void notifyIntern() {
    }

    qore_var_rwlock_priv(const qore_var_rwlock_priv&) = delete;
    qore_var_rwlock_priv& operator=(const qore_var_rwlock_priv&) = delete;
 };

class QoreAutoVarRWReadLocker {
public:
    //! creates the object and grabs the read lock
    DLLLOCAL QoreAutoVarRWReadLocker(QoreVarRWLock &n_l) : l(&n_l) {
        l->rdlock();
    }

    //! creates the object and grabs the read lock
    DLLLOCAL QoreAutoVarRWReadLocker(QoreVarRWLock *n_l) : l(n_l) {
        l->rdlock();
    }

    //! destroys the object and releases the lock
    DLLLOCAL ~QoreAutoVarRWReadLocker() {
        l->unlock();
    }

private:
    QoreAutoVarRWReadLocker(const QoreAutoVarRWReadLocker&) = delete;
    QoreAutoVarRWReadLocker& operator=(const QoreAutoVarRWReadLocker&) = delete;
    void *operator new(size_t) = delete;

protected:
    //! the pointer to the lock that will be managed
    QoreVarRWLock *l;
};

class QoreAutoVarRWWriteLocker {
public:
    //! creates the object and grabs the write lock
    DLLLOCAL QoreAutoVarRWWriteLocker(QoreVarRWLock &n_l) : l(&n_l) {
        l->wrlock();
    }

    //! creates the object and grabs the write lock
    DLLLOCAL QoreAutoVarRWWriteLocker(QoreVarRWLock *n_l) : l(n_l) {
        l->wrlock();
    }

    //! destroys the object and releases the lock
    DLLLOCAL ~QoreAutoVarRWWriteLocker() {
        l->unlock();
    }

protected:
    //! the pointer to the lock that will be managed
    QoreVarRWLock *l;

private:
    QoreAutoVarRWWriteLocker(const QoreAutoVarRWWriteLocker&) = delete;
    QoreAutoVarRWWriteLocker& operator=(const QoreAutoVarRWWriteLocker&) = delete;
    void *operator new(size_t) = delete;
};

class QoreSafeVarRWReadLocker {
public:
    //! creates the object and grabs the read lock
    DLLLOCAL QoreSafeVarRWReadLocker(QoreVarRWLock &n_l) : l(&n_l) {
        l->rdlock();
        locked = true;
    }

    //! creates the object and grabs the read lock
    DLLLOCAL QoreSafeVarRWReadLocker(QoreVarRWLock *n_l) : l(n_l) {
        l->rdlock();
        locked = true;
    }

    //! creates the object and grabs the read lock only if \a do_lock is true
    DLLLOCAL QoreSafeVarRWReadLocker(QoreVarRWLock& n_l, bool do_lock) : l(&n_l), locked(do_lock) {
        if (do_lock) {
            l->rdlock();
        }
    }

    //! destroys the object and releases the lock
    DLLLOCAL ~QoreSafeVarRWReadLocker() {
        if (locked)
            l->unlock();
    }

    //! locks the object and updates the locked flag, assumes that the lock is not already held
    DLLLOCAL void lock() {
        assert(!locked);
        l->rdlock();
        locked = true;
    }

    //! unlocks the object and updates the locked flag, assumes that the lock is held
    DLLLOCAL void unlock() {
        assert(locked);
        locked = false;
        l->unlock();
    }

    //! unlocks the object if the lock is held
    DLLLOCAL void release() {
        if (locked) {
            locked = false;
            l->unlock();
        }
    }

    //! will not unlock the lock when the destructor is run; do not use any other functions of this class after calling this function
    DLLLOCAL void stay_locked() {
        assert(locked);
        locked = false;
    }

protected:
    //! the pointer to the lock that will be managed
    QoreVarRWLock *l;

    //! lock flag
    bool locked;

private:
    QoreSafeVarRWReadLocker(const QoreSafeVarRWReadLocker&) = delete;
    QoreSafeVarRWReadLocker& operator=(const QoreSafeVarRWReadLocker&) = delete;
    void *operator new(size_t) = delete;
};

class QoreSafeVarRWWriteLocker {
private:
   //! this function is not implemented; it is here as a private function in order to prohibit it from being used
   DLLLOCAL QoreSafeVarRWWriteLocker(const QoreSafeVarRWWriteLocker&);

   //! this function is not implemented; it is here as a private function in order to prohibit it from being used
   DLLLOCAL QoreSafeVarRWWriteLocker& operator=(const QoreSafeVarRWWriteLocker&);

   //! this function is not implemented; it is here as a private function in order to prohibit it from being used
   DLLLOCAL void *operator new(size_t);

protected:
   //! the pointer to the lock that will be managed
   QoreVarRWLock *l;

   //! lock flag
   bool locked;

public:
   //! creates the object and grabs the write lock
   DLLLOCAL QoreSafeVarRWWriteLocker(QoreVarRWLock &n_l) : l(&n_l) {
      l->wrlock();
      locked = true;
   }

   //! creates the object and grabs the write lock
   DLLLOCAL QoreSafeVarRWWriteLocker(QoreVarRWLock *n_l) : l(n_l) {
      l->wrlock();
      locked = true;
   }

   //! creates the object and grabs the write lock only if \a do_lock is true
   DLLLOCAL QoreSafeVarRWWriteLocker(QoreVarRWLock& n_l, bool do_lock) : l(&n_l), locked(do_lock) {
      if (do_lock) {
         l->wrlock();
      }
   }

   //! destroys the object and releases the lock
   DLLLOCAL ~QoreSafeVarRWWriteLocker() {
      if (locked)
         l->unlock();
   }

   //! locks the object and updates the locked flag, assumes that the lock is not already held
   DLLLOCAL void lock() {
      assert(!locked);
      l->wrlock();
      locked = true;
   }

   //! unlocks the object and updates the locked flag, assumes that the lock is held
   DLLLOCAL void unlock() {
      assert(locked);
      locked = false;
      l->unlock();
   }

   //! unlocks the object if the lock is held
   DLLLOCAL void release() {
      if (locked) {
         locked = false;
         l->unlock();
      }
   }

   //! will not unlock the lock when the destructor is run; do not use any other functions of this class after calling this function
   DLLLOCAL void stay_locked() {
      assert(locked);
      locked = false;
   }
};

class QoreOptionalVarRWWriteLocker {
protected:
   QoreVarRWLock* l;

public:
   DLLLOCAL QoreOptionalVarRWWriteLocker(QoreVarRWLock* n_l) : l(n_l->trywrlock() ? 0 : n_l) {
   }

   DLLLOCAL QoreOptionalVarRWWriteLocker(QoreVarRWLock& n_l) : l(n_l.trywrlock() ? 0 : &n_l) {
   }

   DLLLOCAL ~QoreOptionalVarRWWriteLocker() {
      if (l)
         l->unlock();
   }

   DLLLOCAL operator bool() const {
      return (bool)l;
   }
};

class QoreOptionalVarRWReadLocker {
protected:
   QoreVarRWLock* l;

public:
   DLLLOCAL QoreOptionalVarRWReadLocker(QoreVarRWLock* n_l) : l(n_l->tryrdlock() ? 0 : n_l) {
   }

   DLLLOCAL QoreOptionalVarRWReadLocker(QoreVarRWLock& n_l) : l(n_l.tryrdlock() ? 0 : &n_l) {
   }

   DLLLOCAL ~QoreOptionalVarRWReadLocker() {
      if (l)
         l->unlock();
   }

   DLLLOCAL operator bool() const {
      return (bool)l;
   }
};

#endif

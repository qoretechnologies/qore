/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_var_rwlock_priv.h

    internal read-write lock for variables

    Qore Programming Language

    Copyright (C) 2003 - 2025 Qore Technologies, s.r.o.

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

// forward reference
//class QoreVarRWLock;

class qore_var_rwlock_priv {
public:
    QoreThreadLock l;
    int write_tid = -1,
        readers = 0,
        read_waiting = 0,
        write_waiting = 0;
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

        while (readers || write_tid != -1) {
            ++write_waiting;
            write_cond.wait(l);
            --write_waiting;
        }

        write_tid = tid;
    }

    //! tries to grab the write lock; does not block if unsuccessful; returns 0 if successful
    DLLLOCAL int trywrlock() {
        int tid = q_gettid();
        assert(tid >= 0);
        AutoLocker al(l);
        assert(tid != write_tid);
        if (readers || write_tid != -1) {
            return -1;
        }

        write_tid = tid;
        return 0;
    }

    //! unlocks the lock (assumes the lock is locked)
    DLLLOCAL void unlock() {
        int tid = q_gettid();
        assert(tid >= 0);
        AutoLocker al(l);
        if (write_tid == tid) {
            write_tid = -1;
            if (has_notify) {
                notifyIntern();
            }

            unlock_signal();
        } else {
            assert(readers);
            if (!--readers) {
                unlock_read_signal();
            }
        }
    }

    //! grabs the read lock
    DLLLOCAL void rdlock() {
        AutoLocker al(l);
        assert(write_tid != q_gettid());
        while (write_tid != -1) {
            ++read_waiting;
            read_cond.wait(l);
            --read_waiting;
        }

        ++readers;
    }

    //! tries to grab the read lock; does not block if unsuccessful; returns 0 if successful
    DLLLOCAL int tryrdlock() {
        AutoLocker al(l);
        assert(write_tid != q_gettid());
        if (write_tid != -1) {
            return -1;
        }

        ++readers;
        return 0;
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

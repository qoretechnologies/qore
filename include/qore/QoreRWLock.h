/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QoreRWLock.h

  simple pthreads-based read-write lock

  Qore Programming Language

  Copyright (C) 2003 - 2023 Qore Technologies, s.r.o.

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

#ifndef _QORE_QORERWLOCK_H
#define _QORE_QORERWLOCK_H

#include <pthread.h>

#ifdef DEBUG
extern int q_gettid() noexcept;
#endif

//! provides a simple POSIX-threads-based read-write lock
/** This utility class is just a simple wrapper for pthread_rwlock_t.  It does
    not provide any special logic for checking for correct usage, etc.
 */
class QoreRWLock {
public:
    //! creates and initializes the lock
    DLLLOCAL QoreRWLock() {
#ifndef NDEBUG
        int rc =
#endif
        pthread_rwlock_init(&m, 0);
        assert(!rc);
    }

    //! destroys the lock
    DLLLOCAL ~QoreRWLock() {
#ifndef NDEBUG
        int rc =
#endif
        pthread_rwlock_destroy(&m);
        assert(!rc);
    }

    //! grabs the write lock
    DLLLOCAL int wrlock() {
#ifndef NDEBUG
        int rc = pthread_rwlock_wrlock(&m);
        if (!rc) {
            wr_tid = q_gettid();
        }
        return rc;
#else
        return pthread_rwlock_wrlock(&m);
#endif
    }

    //! tries to grab the write lock; does not block if unsuccessful; returns 0 if successful
    DLLLOCAL int trywrlock() {
#ifndef NDEBUG
        int rc = pthread_rwlock_trywrlock(&m);
        if (!rc) {
            wr_tid = q_gettid();
        }
        return rc;
#else
        return pthread_rwlock_trywrlock(&m);
#endif
    }

    //! unlocks the lock (assumes the lock is locked)
    DLLLOCAL int unlock() {
#ifndef NDEBUG
        if (wr_tid != -1) {
            wr_tid = -1;
        }
#endif
        return pthread_rwlock_unlock(&m);
    }

    //! grabs the read lock
    DLLLOCAL int rdlock() {
        return pthread_rwlock_rdlock(&m);
    }

    //! tries to grab the read lock; does not block if unsuccessful; returns 0 if successful
    DLLLOCAL int tryrdlock() {
        return pthread_rwlock_tryrdlock(&m);
    }

protected:
    //! the actual locking primitive wrapped in this class
    pthread_rwlock_t m;
#ifndef NDEBUG
    int wr_tid = -1;
#endif

    QoreRWLock& operator=(const QoreRWLock&) = delete;
};

//! provides a safe and exception-safe way to hold read locks in Qore, only to be used on the stack, cannot be dynamically allocated
/** Ensures that read locks are released by locking the read lock when the
    object is created and releasing it when the object is destroyed.
    @see QoreAutoRWWriteLocker
*/
class QoreAutoRWReadLocker {
private:
    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreAutoRWReadLocker(const QoreAutoRWReadLocker&);

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreAutoRWReadLocker& operator=(const QoreAutoRWReadLocker&);

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL void *operator new(size_t);

protected:
    //! the pointer to the lock that will be managed
    QoreRWLock *l;

public:
    //! creates the object and grabs the read lock
    DLLLOCAL QoreAutoRWReadLocker(QoreRWLock &n_l) : l(&n_l) {
        l->rdlock();
    }

    //! creates the object and grabs the read lock. If parameter is null then no function is performed.
    DLLLOCAL QoreAutoRWReadLocker(QoreRWLock *n_l) : l(n_l) {
        if (l)
            l->rdlock();
    }

    //! destroys the object and releases the lock
    DLLLOCAL ~QoreAutoRWReadLocker() {
        if (l)
            l->unlock();
    }
};

//! provides a safe and exception-safe way to hold write locks in Qore, only to be used on the stack, cannot be dynamically allocated
/** Ensures that write locks are released by locking the write lock when the
    object is created and releasing it when the object is destroyed.
    @see QoreAutoRWReadLocker
*/
class QoreAutoRWWriteLocker {
private:
    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreAutoRWWriteLocker(const QoreAutoRWWriteLocker&);

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreAutoRWWriteLocker& operator=(const QoreAutoRWWriteLocker&);

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL void *operator new(size_t);

protected:
    //! the pointer to the lock that will be managed
    QoreRWLock *l;

public:
    //! creates the object and grabs the write lock
    DLLLOCAL QoreAutoRWWriteLocker(QoreRWLock &n_l) : l(&n_l) {
        l->wrlock();
    }

    //! creates the object and grabs the write lock. If parameter is null then no function is performed.
    DLLLOCAL QoreAutoRWWriteLocker(QoreRWLock *n_l) : l(n_l) {
        if (l)
            l->wrlock();
    }

    //! destroys the object and releases the lock
    DLLLOCAL ~QoreAutoRWWriteLocker() {
        if (l)
            l->unlock();
    }
};

//! provides a safe and exception-safe way to hold read locks in Qore, only to be used on the stack, cannot be dynamically allocated
/** Ensures that read locks are released by locking the read lock when the
    object is created and releasing it when the object is destroyed.
    @see QoreSafeRWWriteLocker
*/
class QoreSafeRWReadLocker {
public:
    //! creates an empty object
    DLLLOCAL QoreSafeRWReadLocker() : l(nullptr), locked(false) {
    }

    //! creates the object and grabs the read lock
    DLLLOCAL QoreSafeRWReadLocker(QoreRWLock& n_l) : l(&n_l) {
        l->rdlock();
        locked = true;
    }

    //! creates the object and grabs the read lock
    DLLLOCAL QoreSafeRWReadLocker(QoreRWLock* n_l) : l(n_l) {
        l->rdlock();
        locked = true;
    }

    //! destroys the object and releases the lock
    DLLLOCAL ~QoreSafeRWReadLocker() {
        if (locked) {
            l->unlock();
        }
    }

    //! locks the object and updates the locked flag, assumes that the lock is not already held
    DLLLOCAL void lock() {
        assert(!locked);
        assert(l);
        l->rdlock();
        locked = true;
    }

    //! unlocks the object and updates the locked flag, assumes that the lock is held
    DLLLOCAL void unlock() {
        assert(locked);
        assert(l);
        locked = false;
        l->unlock();
    }

    //! will not unlock the lock when the destructor is run; do not use any other functions of this class after calling this function
    DLLLOCAL void stay_locked() {
        assert(locked);
        assert(l);
        locked = false;
    }

    //! Handoff to another read lock
    DLLLOCAL void handoffTo(QoreRWLock& n) {
        if (l) {
            unlock();
        }
        l = &n;
        lock();
    }

protected:
    //! the pointer to the lock that will be managed
    QoreRWLock* l;

    //! lock flag
    bool locked;

private:
    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreSafeRWReadLocker(const QoreSafeRWReadLocker&) = delete;

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreSafeRWReadLocker& operator=(const QoreSafeRWReadLocker&) = delete;

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL void* operator new(size_t) = delete;
};

//! provides a safe and exception-safe way to hold write locks in Qore, only to be used on the stack, cannot be dynamically allocated
/** Ensures that write locks are released by locking the write lock when the
    object is created and releasing it when the object is destroyed.
    @see QoreSafeRWReadLocker
*/
class QoreSafeRWWriteLocker {
private:
    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreSafeRWWriteLocker(const QoreSafeRWWriteLocker&);

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreSafeRWWriteLocker& operator=(const QoreSafeRWWriteLocker&);

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL void *operator new(size_t);

protected:
    //! the pointer to the lock that will be managed
    QoreRWLock *l;

    //! lock flag
    bool locked;

public:
    //! creates the object and grabs the write lock
    DLLLOCAL QoreSafeRWWriteLocker(QoreRWLock &n_l) : l(&n_l) {
        l->wrlock();
        locked = true;
    }

    //! creates the object and grabs the write lock
    DLLLOCAL QoreSafeRWWriteLocker(QoreRWLock *n_l) : l(n_l) {
        l->wrlock();
        locked = true;
    }

    //! destroys the object and releases the lock
    DLLLOCAL ~QoreSafeRWWriteLocker() {
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

class QoreOptionalRWWriteLocker {
protected:
    QoreRWLock* l;

public:
    DLLLOCAL QoreOptionalRWWriteLocker(QoreRWLock* n_l) : l(n_l->trywrlock() ? 0 : n_l) {
    }

    DLLLOCAL QoreOptionalRWWriteLocker(QoreRWLock& n_l) : l(n_l.trywrlock() ? 0 : &n_l) {
    }

    DLLLOCAL ~QoreOptionalRWWriteLocker() {
        if (l)
            l->unlock();
    }

    DLLLOCAL operator bool() const {
        return (bool)l;
    }
};

class QoreOptionalRWReadLocker {
protected:
    QoreRWLock* l;

public:
    DLLLOCAL QoreOptionalRWReadLocker(QoreRWLock* n_l) : l(n_l->tryrdlock() ? 0 : n_l) {
    }

    DLLLOCAL QoreOptionalRWReadLocker(QoreRWLock& n_l) : l(n_l.tryrdlock() ? 0 : &n_l) {
    }

    DLLLOCAL ~QoreOptionalRWReadLocker() {
        if (l)
            l->unlock();
    }

    DLLLOCAL operator bool() const {
        return (bool)l;
    }
};

class qore_var_rwlock_priv;

class QoreVarRWLock {
private:
    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreVarRWLock(const QoreVarRWLock&);
    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreVarRWLock& operator=(const QoreVarRWLock&);

protected:
    qore_var_rwlock_priv* priv;

    DLLLOCAL QoreVarRWLock(qore_var_rwlock_priv* p);

public:
    DLLLOCAL QoreVarRWLock();

    //! destroys the lock
    DLLLOCAL ~QoreVarRWLock();

    //! grabs the write lock
    DLLLOCAL void wrlock();

    //! tries to grab the write lock; does not block if unsuccessful; returns 0 if successful
    DLLLOCAL int trywrlock();

    //! unlocks the lock (assumes the lock is locked)
    DLLLOCAL void unlock();

    //! grabs the read lock
    DLLLOCAL void rdlock();

    //! tries to grab the read lock; does not block if unsuccessful; returns 0 if successful
    DLLLOCAL int tryrdlock();
};

#endif

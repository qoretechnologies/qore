/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QoreRWLock.h

  simple pthreads-based read-write lock

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

#ifndef _QORE_QORERWLOCK_H
#define _QORE_QORERWLOCK_H

class qore_var_rwlock_priv;

//! provides a read-write lock that always prefers readers
/** The lock is not a wrapper for \c pthread_rwlock_t, because reader/writer preference is not
    portably configurable there: \c pthread_rwlockattr_setkind_np() is a glibc extension, and
    Darwin's \c pthread_rwlockattr_t supports only \c pshared, so a \c pthread_rwlock_t is
    writer-preferring on macOS and reader-preferring on Linux with no way to align the two.  The
    policy is therefore implemented by Qore itself, so that the semantics are identical on all
    platforms.

    Qore code assumes reader preference, which is what makes it safe for a thread already holding
    the read lock to acquire it again (directly or through a nested call): a reader waits only
    while a writer actually \a holds the lock, never because a writer is queued behind it.  Under
    writer preference the second acquisition blocks behind the queued writer while the writer waits
    for the first acquisition to be released, which deadlocks.

    A thread holding the read lock must not acquire the write lock, and a thread holding the write
    lock must not acquire either lock again; both are asserted in debug builds.

    This class does not provide any other logic for checking for correct usage, etc.

    @note the implementation is private (see \c qore_var_rwlock_priv), so that the lock's internal
    layout is not baked into binary modules; the same implementation backs QoreVarRWLock.  When
    debugging a live process or a core dump, \c priv->write_tid gives the TID of the thread holding
    the write lock (-1 if none), and \c priv->readers the number of threads holding the read lock
 */
class QoreRWLock {
public:
    //! creates and initializes the lock
    DLLEXPORT QoreRWLock();

    //! destroys the lock
    DLLEXPORT ~QoreRWLock();

    //! grabs the write lock; returns 0 (the return value exists for API compatibility)
    DLLEXPORT int wrlock();

    //! tries to grab the write lock; does not block if unsuccessful; returns 0 if successful
    DLLEXPORT int trywrlock();

    //! unlocks the lock (assumes the lock is locked)
    /** releases the write lock if the current thread holds it, otherwise releases a read lock
    */
    DLLEXPORT int unlock();

    //! grabs the read lock; returns 0 (the return value exists for API compatibility)
    DLLEXPORT int rdlock();

    //! tries to grab the read lock; does not block if unsuccessful; returns 0 if successful
    DLLEXPORT int tryrdlock();

protected:
    //! the private implementation of the lock
    qore_var_rwlock_priv* priv;

private:
    QoreRWLock(const QoreRWLock&) = delete;
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
    friend class qore_var_rwlock_priv;
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

protected:
    qore_var_rwlock_priv* priv;

    DLLLOCAL QoreVarRWLock(qore_var_rwlock_priv* p);

private:
    QoreVarRWLock(const QoreVarRWLock&) = delete;
    QoreVarRWLock& operator=(const QoreVarRWLock&) = delete;
};

#endif

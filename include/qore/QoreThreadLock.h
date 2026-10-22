/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreThreadLock.h

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

#ifndef _QORE_QORETHREADLOCK_H

#define _QORE_QORETHREADLOCK_H

#include <cassert>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>

//! provides a mutually-exclusive thread lock
/** This class is just a simple wrapper for pthread_mutex_t.  It does not provide any special
    logic for checking for correct usage, etc.

    @see QoreRecursiveThreadLock
*/
class QoreThreadLock {
    friend class QoreCondition;

public:
    //! creates the lock
    DLLLOCAL QoreThreadLock() : QoreThreadLock(nullptr) {
    }

    //! creates the lock with the given attributes
    DLLLOCAL QoreThreadLock(const pthread_mutexattr_t* ma) {
#ifndef NDEBUG
        int rc =
#endif
        pthread_mutex_init(&ptm_lock, ma);
        assert(!rc);
    }

    //! creates a new object (not based on the original lock status)
    DLLLOCAL QoreThreadLock(const QoreThreadLock&) : QoreThreadLock(nullptr) {
    }

    //! destroys the lock
    DLLLOCAL ~QoreThreadLock() {
        pthread_mutex_destroy(&ptm_lock);
    }

    //! grabs the lock (assumes that the lock is unlocked)
    /** no error checking happens here; if you grab the lock twice it will deadlock
    */
    DLLLOCAL void lock() {
#ifndef NDEBUG
        int rc =
#endif
        pthread_mutex_lock(&ptm_lock);
        assert(!rc);
    }

    //! releases the lock (assumes that the lock is locked)
    /** no error checking is implemented here
    */
    DLLLOCAL void unlock() {
#ifndef NDEBUG
        int rc =
#endif
        pthread_mutex_unlock(&ptm_lock);
        assert(!rc);
    }

    //! attempts to acquire the mutex and returns the status immediately; does not block
    /**
        @return 0 if the lock was acquired, a non-zero error number if the lock was not acquired
    */
    DLLLOCAL int trylock() {
        return pthread_mutex_trylock(&ptm_lock);
    }

    //! initializes the lock again, unlocked, in a child process created with fork() (internal)
    /** Only the thread that called fork() exists in the child, so a lock held by another thread of the parent would
        stay locked forever; see QoreForkThreadLock

        @param ma the attributes the lock was created with
    */
    DLLLOCAL void reinitAfterFork(const pthread_mutexattr_t* ma = nullptr) {
        // reset to the static initializer first: an implementation may refuse to initialize a lock that is held
        // (macOS does for read-write locks)
        static const pthread_mutex_t unlocked = PTHREAD_MUTEX_INITIALIZER;
        ptm_lock = unlocked;
#ifndef NDEBUG
        int rc =
#endif
        pthread_mutex_init(&ptm_lock, ma);
        assert(!rc);
    }

private:
    //! the actual locking primitive wrapped in this class
    pthread_mutex_t ptm_lock;

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL QoreThreadLock& operator=(const QoreThreadLock&) = delete;
};

//! Implements a recursive lock
/** @since %Qore 0.9.5
*/
class QoreRecursiveThreadLock : public QoreThreadLock {
public:
    //! Creates the object
    DLLEXPORT QoreRecursiveThreadLock();

    //! Creates a new object (not based on the original lock status)
    DLLLOCAL QoreRecursiveThreadLock(const QoreRecursiveThreadLock&) : QoreRecursiveThreadLock() {
    }
};

//! provides a safe and exception-safe way to hold locks in Qore, only to be used on the stack, cannot be dynamically allocated
/** Ensures that locks are released by locking the lock when the
    object is created and releasing it when the object is destroyed.
    For a similar object that allows for unlocking the lock earlier
    than the object's destruction, see SafeLocker.

    @see AutoUnlocker
    @see SafeLocker
    @see OptLocker
*/
class AutoLocker {
public:
    //! creates the object and grabs the lock
    DLLLOCAL AutoLocker(QoreThreadLock* l) : lck(l) {
        lck->lock();
    }

    //! creates the object and grabs the lock
    DLLLOCAL AutoLocker(QoreThreadLock& l) : lck(&l) {
        lck->lock();
    }

    //! creates the object and grabs the lock
    /** @since %Qore 0.9
    */
    DLLLOCAL AutoLocker(QoreThreadLock* l, bool already_locked) : lck(l) {
        if (!already_locked) {
            lck->lock();
        }
    }

    //! creates the object and grabs the lock
    /** @since %Qore 0.9
    */
    DLLLOCAL AutoLocker(QoreThreadLock& l, bool already_locked) : lck(&l) {
        if (!already_locked) {
            lck->lock();
        }
    }

    //! destroys the object and releases the lock
    DLLLOCAL ~AutoLocker() {
        lck->unlock();
    }

private:
    DLLLOCAL AutoLocker(const AutoLocker&) = delete;
    DLLLOCAL AutoLocker& operator=(const AutoLocker&) = delete;
    DLLLOCAL void *operator new(size_t) = delete;

protected:
    //! the pointer to the lock that will be managed
    QoreThreadLock* lck;
};

//! provides a safe and exception-safe way to release and re-acquire locks in Qore, only to be used on the stack, cannot be dynamically allocated
/** Ensures that locks are released when the object is created and re-acquired when the object is destroyed.

    @see AutoLocker
    @see SafeLocker
    @see OptLocker

    @since Qore 0.8.10
*/
class AutoUnlocker {
public:
    //! creates the object and releases the lock
    DLLLOCAL AutoUnlocker(QoreThreadLock* l) : lck(l) {
        if (lck)
            lck->unlock();
    }

    //! creates the object and releases the lock
    DLLLOCAL AutoUnlocker(QoreThreadLock& l) : lck(&l) {
        lck->unlock();
    }

    //! grabs the lock and destroys the object
    DLLLOCAL ~AutoUnlocker() {
        if (lck)
            lck->lock();
    }

private:
    DLLLOCAL AutoUnlocker(const AutoUnlocker&) = delete;
    DLLLOCAL AutoUnlocker& operator=(const AutoUnlocker&) = delete;
    DLLLOCAL void *operator new(size_t) = delete;

protected:
    //! the pointer to the lock that will be managed
    QoreThreadLock* lck;
};

//! provides an exception-safe way to manage locks in Qore, only to be used on the stack, cannot be dynamically allocated
/** Ensures that locks are released by locking the lock when the
    object is created and releasing it when the object is destroyed.
    Also allows the lock to be released before the object's destruction
    at the expense of one extra byte on the stack compared to the AutoLocker class.

    @see AutoLocker
    @see OptLocker
*/
class SafeLocker {
public:
    //! creates the object and grabs the lock
    DLLLOCAL SafeLocker(QoreThreadLock* l) : lck(l) {
        lck->lock();
        locked = true;
    }

    //! creates the object and grabs the lock
    DLLLOCAL SafeLocker(QoreThreadLock& l) : lck(&l) {
        lck->lock();
        locked = true;
    }

    //! destroys the object and unlocks the lock if it's held
    DLLLOCAL ~SafeLocker() {
        if (locked)
            lck->unlock();
    }

    //! locks the object and updates the locked flag, assumes that the lock is not already held
    DLLLOCAL void lock() {
        assert(!locked);
        lck->lock();
        locked = true;
    }

    //! unlocks the object and updates the locked flag, assumes that the lock is held
    DLLLOCAL void unlock() {
        assert(locked);
        locked = false;
        lck->unlock();
    }

    //! will not unlock the lock when the destructor is run; do not use any other functions of this class after calling this function
    DLLLOCAL void stay_locked() {
        assert(locked);
        locked = false;
    }

    //! relocks an unlock lock
    DLLLOCAL void relock() {
        assert(!locked);
        lck->lock();
        locked = true;
    }

private:
    DLLLOCAL SafeLocker(const SafeLocker&) = delete;
    DLLLOCAL SafeLocker& operator=(const SafeLocker&) = delete;
    DLLLOCAL void *operator new(size_t) = delete;

protected:
    //! the pointer to the lock that will be managed
    QoreThreadLock* lck;

    //! flag indicating if the lock is held or not
    bool locked;
};

//! provides a safe and exception-safe way to hold optional locks in Qore, only to be used on the stack, cannot be dynamically allocated
/** Ensures that locks are released by locking the lock when the object is created and releasing it when the object is destroyed.
    @see SafeLocker
    @see AutoLocker
*/
class OptLocker {
public:
    //! creates the object and grabs the lock if the argument is not NULL
    DLLLOCAL OptLocker(QoreThreadLock* l) : lck(l) {
        if (lck)
            lck->lock();
    }

    //! releases the lock if there is a lock pointer being managed and destroys the object
    DLLLOCAL ~OptLocker() {
        if (lck)
            lck->unlock();
    }

private:
    DLLLOCAL OptLocker(const OptLocker&) = delete;
    DLLLOCAL OptLocker& operator=(const OptLocker&) = delete;
    DLLLOCAL void *operator new(size_t) = delete;

protected:
    //! the pointer to the lock that will be managed
    QoreThreadLock* lck;
};

#endif // _QORE_QORETHREADLOCK_H

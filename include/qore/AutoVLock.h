/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  AutoVLock.h

  Qore Programming Language

  Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

#ifndef _QORE_AUTOVLOCK_H

#define _QORE_AUTOVLOCK_H

class AbstractSmartLock;

struct QLckPtr {
public:
    DLLLOCAL QLckPtr() : rwl(0) {
    }

    DLLLOCAL void set(QoreVarRWLock* n_rwl) {
        rwl = n_rwl;
    }

    DLLLOCAL QoreVarRWLock* getRWL() const {
        return rwl;
    }

    DLLLOCAL bool isSet() const {
        return rwl;
    }

    DLLLOCAL void unlockAndClear() {
        if (rwl) {
            unlockIntern();
            rwl = 0;
        }
    }

    DLLLOCAL void clear() {
        assert(rwl);
        rwl = 0;
    }

protected:
    DLLLOCAL void unlockIntern() {
        assert(rwl);
        rwl->unlock();
    }

private:
    QoreVarRWLock* rwl;
};

//! AutoVLock is a container for safely managing global variable and object lock handovers, required for functions accessing global variables and object data where locking is necessary
/** This object is used for lock handover management and automatically releasing the last lock.
 */
class AutoVLock {
    // Internal lvalue handoff preserves pending notifications until the operation has finished.
    friend class LValueLockHandoffHelper;
private:
    // pointer to lock currently held
    QLckPtr lock;

    // pointer to object to dereference
    QoreObject* o;

public:
    // pointer to ExceptionSink object for use with object notifications
    ExceptionSink* xsink;

private:
    //! private implementation of the object notification container
    struct qore_avl_private* priv;

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL AutoVLock(const AutoVLock&) = delete;
    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL AutoVLock& operator=(const AutoVLock&) = delete;
    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL void* operator new(size_t) = delete;

public:
    //! creates an empty lock container
    /** @param n_xsink pointer to ExceptionSink object for use with object notifications
    */
    DLLEXPORT AutoVLock(ExceptionSink* n_xsink);

    //! default move constructor
    DLLLOCAL AutoVLock(AutoVLock&& o) = default;

    //! releases all locks held and destroys the container
    DLLEXPORT ~AutoVLock();

    //! returns true if managing a lock, false if not (unlocked)
    DLLEXPORT operator bool() const;

    //! manually releases the lock currently held
    DLLEXPORT void del();

    //! sets the current lock
    DLLLOCAL void set(QoreVarRWLock* n_rwl);

    //! sets the current object (for dereference) and lock
    DLLLOCAL void set(QoreObject* n_o, QoreVarRWLock* n_rwl);

    //! gets the current read-write lock
    DLLLOCAL QoreVarRWLock* getRWL() const;

    //! gets the current object
    DLLLOCAL QoreObject* getObject() const;

    //! leaves the lock locked and the object referenced and clears the object and lock pointers
    DLLLOCAL void clear();

    //! adds an object member notification entry, internal-only
    /** @param o the object to add
         @param member the member that was changed (must be in QCS_DEFAULT encoding)
    */
    DLLLOCAL void addMemberNotification(QoreObject* o, const char* member);
};

#endif
